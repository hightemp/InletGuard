# PRD: InletGuard — PHP extension protecting against injection attacks

> **Baseline English version.** The authoritative document is
> [`PRD.ru.md`](PRD.ru.md) (Russian). This file follows it section by section
> in condensed form; if they disagree, the Russian version wins.
> Every change goes into both files in the same commit.

> **Development method: TDD.** No production code without a failing test
> first (red → green → refactor). See section 9 and [`TASKS.md`](TASKS.md).

> **Languages.** The project is in **English**: code, identifiers, comments,
> log and error messages, tests, commits, `README.md`, `docs/`, ADRs. Only
> `PRD.ru.md` and `TASKS.ru.md` are in Russian.

> **Supported PHP: 7.0 – 8.5**, NTS and ZTS, Linux. The whole 7.x branch is
> covered because shared hosting still runs it widely (cPanel / CloudLinux
> alt-php, ISPmanager). PHP 5.x is not supported (different Zend API).

## 1. Overview

InletGuard is a native PHP extension written in C (loaded like `curl` or
`gd`: `extension=inletguard.so`). At the interpreter level it:

- inspects request input (`$_GET`, `$_POST`, `$_COOKIE`, URI, headers) for
  attacks **before the PHP script runs**;
- hooks SQL functions (`mysqli`, `PDO`, `pgsql`, `sqlite3`) and checks whether
  user input changed the structure of the query (RASP);
- exposes PHP functions for manual checks;
- reacts according to configuration: **monitor** (log only), **block**
  (HTTP 403, stop execution), **sanitize** (clean the value);
- writes events to a log file.

Detection engine: bundled **libinjection** (BSD-3-Clause) plus own input
normalization.

## 2. Goals and non-goals

### Goals (v1.0)
1. Detect **SQL injection** and **XSS** in request input with a low false
   positive rate.
2. Detect SQL injection at query execution time (function hooks).
3. Configurable reaction: `monitor` / `block` / `sanitize`, globally and per
   attack class.
4. JSON Lines event log.
5. No application code changes required.
6. Stability: never crash the PHP process; fail open on internal errors.
7. **Works out of the box:** `extension=inletguard.so` is the only required
   setting (FR-13).
8. **CI and release pipeline from day one;** publishing starts with the first
   working version, v1.0.0 (section 9.2).

### Long-term goal
Cover **every attack class detectable at the PHP runtime level** (section 10).

### Not in v1.0
- Attack classes other than SQLi and XSS; JSON/XML body inspection.

### Project non-goals
- Network/transport attacks (request smuggling, TLS, network DDoS).
- CSRF, IDOR, broken access control, business logic flaws (need application
  semantics; only heuristics are in the backlog).
- Application crypto mistakes, supply chain.
- Windows, PHP 5.x. Web UI, database storage of events, SIEM export.
- Replacing a WAF or parameterized queries: InletGuard is an extra layer.

## 3. Users and scenarios

| Role | Scenario |
|---|---|
| Site owner / hosting customer | Installs the package (or adds `extension=inletguard.so`) and configures nothing. |
| Server admin | Optionally runs `monitor` + `learn` first, then a stricter profile. |
| Developer | Calls `InletGuard\check_sqli()` / `InletGuard\check_xss()`, excludes HTML fields. |
| Security engineer | Analyzes the JSON log, tunes exclusions and IGR rules. |

**Main scenario — install and forget:** protection is active with the
`balanced` profile (high-precision checks block, false-positive-prone checks
only log); events go to the standard `error_log`.

**Careful scenario:** `inletguard.mode=monitor` + `inletguard.learn.enabled=1`
for 1–2 weeks → `inletguard-learn` → apply exclusions → remove `mode=monitor`.

## 4. Functional requirements (summary)

- **FR-1 Request input inspection.** Sources: GET, POST (urlencoded,
  multipart text fields), COOKIE, URI path, query string, selected headers.
  Keys and values, nested arrays (depth limit). Normalization: repeated URL
  decoding, NUL removal, HTML entity decoding for XSS. Length and count
  limits. Runs before the script. Disabled in CLI by default.
- **FR-2 Detectors.** `libinjection_sqli` (fingerprint is logged),
  `libinjection_xss` plus our own check for a dangerous URL scheme at the
  start of a value (`javascript:`, `vbscript:`, HTML/SVG `data:`), which
  libinjection misses (note 1.2); a common detector interface for future
  classes.
- **FR-3 SQL hooks (RASP).** `mysqli_*`, `PDO::query/exec/prepare`, `pg_*`,
  `SQLite3::*`. A user value found verbatim in the query must fit inside a
  single literal token; leaking into operators/keywords/comments is an
  injection. Block → `InletGuard\SqlInjectionException` or `false`.
- **FR-4 Reactions.** `monitor`, `block` (status, HTML/JSON/text body
  template with `{request_id}`, optional `X-InletGuard-Request-Id` header —
  off by default), `sanitize` with per-module strategies
  (`xss.sanitize = escape|strip_tags|empty`, `sqli.sanitize = empty|strip`).
  The strictest reaction wins.
- **FR-5 Exclusions.** `exclude_params`, `exclude_paths`, per-module
  `<m>.exclude_*`, glob patterns.
- **FR-6 Logging.** JSON Lines (or `logfmt`) to `inletguard.log_file`, else
  `error_log`. Safe JSON escaping, atomic writes, never breaks the request.
  Rotation: `external` (logrotate + inode check), `size`, `daily`, `hourly`.
  Flood protection (`log_rate_limit`, `log_dedup_window`). Privacy
  (`log_fields_exclude`, `log_ip_anonymize`, `log_payload=0`).
- **FR-7 PHP API.** `InletGuard\check_sqli()`, `check_xss()`, `inspect()`,
  `events()`, `request_id()`, `version()`, `config()`, `modules()`,
  `tighten()`; constants; `SqlInjectionException`. API never blocks.
  Signatures limited to PHP 7.0 capabilities.
- **FR-8 Configuration.** All directives are `PHP_INI_SYSTEM`: the
  application cannot weaken protection via `ini_set()`, `.user.ini` or
  `.htaccess`. Allowed in `php.ini`/`conf.d`, FPM pool and Apache vhost
  (`php_admin_value`). Invalid values → default + `config_warning`.
- **FR-10 Modularity.** See below.
- **FR-11 Learning mode.** See below.
- **FR-12 Counter storage.** See below.
- **FR-13 Zero-config.** See below.

### FR-10 Modularity and configuration model
- **Module = attack class**, with a stable ID (`sqli`, `xss`, `lfi`, `cmdi`,
  `ssrf`, ...), registered in a module registry; the core knows no concrete
  module.
- **Four levels of switching off:**

  | Level | Mechanism | Effect |
  |---|---|---|
  | Build | `--with-inletguard-modules=...`, `--disable-inletguard-<m>` | Module code is not in the `.so` |
  | Startup | `inletguard.modules = all,-ssti` | No hooks installed, zero overhead |
  | Runtime | `inletguard.<m>.enabled`, `.input/.sink/.output.enabled` | Per request; per FPM pool / vhost |
  | Request | IGR rules `allow`, `set_mode` | By path, host, IP, method |

- **Uniform per-module directives:** `enabled`, `mode`, `input/sink/output`
  switches and modes, per-source mode `<m>.input.<source>.mode`, `level`
  (1–4 sensitivity), `exclude_params`, `exclude_paths`, `sanitize`, `log`,
  `block_status`, `block_page`, plus module-specific options.
- **Profiles:** `off`, `minimal`, `balanced` (default), `strict`, `paranoid`.
  Explicit directives override the profile.
- **Precedence:** defaults < profile < global < module < pool/vhost < IGR
  request rules.
- **Tightening from the app** (opt-in): `InletGuard\tighten()` can only make
  protection stricter.
- **Validation/introspection:** unknown `inletguard.*` keys reported with
  "did you mean", `config_strict`, value sources in `phpinfo()`,
  `InletGuard\config()`, `inletguard-config check`, example ini files.

### FR-11 Learning mode (`learn` module)
Adds analysis fields to events (`path_template`, response `status`,
`request_attacks`, `trusted`), downgrades blocking while learning, and a CLI
`inletguard-learn` proposes exclusion candidates (ini or IGR) with confidence
levels. Never applied automatically; protected against poisoning (thresholds
on distinct IPs/days, scanners and attacking IPs ignored, trusted-IP mode).

### FR-12 Counter storage (behavior level)
Interface `ig_counter_store_t`; backends `shm` (default, named per-user
segment, v2.2), `redis` and `memcached` (v2.3, own minimal clients),
`hybrid` mode, fallback to local shm when the backend is down.

### FR-13 Zero-config
Only `extension=inletguard.so` is needed. Profile `balanced`: sink checks and
input SQLi **block**; XSS **blocks** in GET/COOKIE/URI/headers and **logs** in
POST/JSON (forms with legitimate HTML); noisy modules log; output and
behavior modules off. Events go to `error_log`; rules directory
`/etc/inletguard/rules.d/*.igr` is optional. Covered by an end-to-end test.

## 5. Non-functional requirements

| ID | Requirement |
|---|---|
| NFR-1 Performance | < 50 µs per typical request (20 params); < 10 µs per DB query without matches |
| NFR-2 Reliability | No crashes or leaks: ASan/UBSan, Valgrind, ≥ 1 h fuzzing before release |
| NFR-3 Fail-open | Internal error → `internal_error` event, request continues (`fail_mode`) |
| NFR-4 Compatibility | PHP 7.0–8.5 × NTS/ZTS; FPM, CGI, mod_php, CLI, LSAPI; ext/filter; x86_64, aarch64; version checks only in `php_compat.h` |
| NFR-5 Accuracy | SQLi detection ≥ 95 %, XSS ≥ 90 %, false positives < 0.5 % on corpora |
| NFR-6 Code safety | `-Wall -Wextra -Werror`, static analysis, no unbounded string functions |
| NFR-7 Build | `phpize && ./configure --enable-inletguard && make && make test`; C99, GCC ≥ 4.8; PECL and PIE |
| NFR-8 Language | Everything in English except `PRD.ru.md`, `TASKS.ru.md` |
| NFR-9 License | MIT; third-party code only MIT/BSD/Apache-2.0/zlib, no GPL/LGPL; `THIRD_PARTY_NOTICES`; SPDX headers |
| NFR-10 Modularity | A disabled module has no behavioral or performance effect |
| NFR-11 Portable binaries | Release `.so` files built against glibc 2.17 |

## 6. Architecture

```
HTTP request ─▶ input filter hook ─▶ Inspector ─▶ Normalizer ─▶ Detectors (libinjection)
                                         │
                                         ▼
                        Policy (modes, exclusions) ─▶ Action: log / block / sanitize
SQL call     ─▶ SQL hooks ───────────────┘ (structural query check)
PHP API      ─▶ InletGuard\* functions
                                         ▼
                                Logger (JSON Lines)
```

- `src/core/` — pure C, no Zend: normalizer, detector wrappers, JSON,
  glob, tokenizers. Unit-tested — the base of the TDD loop.
- `src/zend/` — Zend integration (ini, hooks, superglobals, API).
- Per-request state in module globals (ZTS-safe).
- **Source → sink model:** a registry of user input (taint-lite,
  Aho–Corasick search), a declarative sink registry (function, argument,
  grammar, attack class) with one hook dispatcher, and grammar tokenizers
  answering "does user input leave a single literal token?". A new attack
  class is a registry entry + tokenizer + input signatures.

## 7. Success metrics
NFR-5 met on corpora; zero known crashes/leaks at release; a week on a test
stand (WordPress + legacy app) without false blocks after tuning.

## 8. Risks (summary)
False positives on rich HTML; encoding bypasses; conflicts with other
extensions hooking the same functions (Xdebug, APM); Zend API differences
across 7.0–8.5; C bugs crashing sites; aging images for EOL PHP versions;
overhead of many hooks; SSRF bypasses (DNS rebinding, URL parser
differentials); libinjection does not catch everything (published SQLi bypasses,
weaker XSS part, bare `javascript:` URLs) — kept by decision of 2026-10-22 as
one signal among several, isolated behind the detector interface, and reviewed
against corpus metrics (1.8). Mitigations: monitor/learn, corpora, fuzzing, sanitizers,
`php_compat.h`, CI matrix, performance budgets — see `PRD.ru.md` section 8.

## 9. Testing strategy (TDD)

Red → green → refactor for every task. Levels: C unit tests (core), `.phpt`
integration tests (same tests for all PHP versions), corpora (accuracy),
ASan/UBSan and Valgrind, libFuzzer, performance budgets.

**Definition of done:** tests written before code, green on PHP 7.0–8.5 ×
NTS/ZTS, ASan clean, no compiler warnings.

### 9.1 CI (GitHub Actions)
Built in phase 0, before any production code.

| Workflow | Trigger | Content |
|---|---|---|
| `ci.yml` | push, PR | lint, license/SPDX, unit tests, phpt matrix 7.0–8.5 × NTS/ZTS, ASan, Valgrind, old toolchain, aarch64 |
| `nightly.yml` | cron | fuzzing, corpus metrics, benchmarks |
| `release.yml` | tag, manual, push to `main` (dry-run) | build, package, smoke-test, publish per 9.2 |

Actions pinned by commit SHA, Dependabot updates.

### 9.2 Release pipeline and publishing policy
- Artifacts: `.so` for 10 PHP versions × NTS/ZTS × x86_64/aarch64 built on
  glibc 2.17; archives with `.so`, `20-inletguard.ini`, `README.md`,
  `LICENSE`, `THIRD_PARTY_NOTICES`; PECL package; `SHA256SUMS`; build
  attestations. Smoke-tested on CentOS 7, AlmaLinux 8–9, Debian, Ubuntu.
- **No releases are published before v1.0.0** (phases 0–7 done). Until then
  `release.yml` only runs in dry-run mode. Publishing needs both
  `RELEASES_ENABLED=true` and a tag `≥ v1.0.0`; `v0.*` tags are rejected;
  releases are drafts that require maintainer approval.

## 10. Attack class coverage roadmap

Detection levels: **Input** (request inspection), **Sink** (RASP at dangerous
functions), **Output** (response analysis), **Behavior** (cross-request
counters).

| # | Class | Level | Release |
|---|---|---|---|
| 1 | SQL injection | Input + Sink | v1.0 |
| 2 | XSS (input) | Input | v1.0 |
| 3 | HTTP parameter pollution, input anomalies | Input | v1.1 |
| 4 | Scanners and reconnaissance | Input | v1.1 |
| 5 | Application DoS via input (JSON depth, sizes) | Input | v1.1 |
| 6 | Path traversal / LFI | Input + Sink | v1.2 |
| 7 | RFI and dangerous stream wrappers | Sink | v1.2 |
| 8 | Malicious file uploads | Input + Sink | v1.2 |
| 9 | Webshell drop and execution | Sink | v1.2 |
| 10 | Zip Slip | Sink | v1.2 |
| 11 | OS command injection | Input + Sink | v1.3 |
| 12 | Argument / environment injection (`mail()`, `putenv`, `imap_open`) | Sink | v1.3 |
| 13 | Code injection (`eval`, `assert`, `create_function`, dynamic calls) | Input + Sink | v1.3 |
| 14 | Insecure deserialization / object injection, phar | Input + Sink | v1.4 |
| 15 | XXE and XML bombs | Input + Sink | v1.4 |
| 16 | SSRF | Sink | v1.4 |
| 17 | LDAP injection | Sink | v1.5 |
| 18 | XPath injection | Sink | v1.5 |
| 19 | NoSQL injection | Input + Sink | v1.5 |
| 20 | SSTI | Input | v1.5 |
| 21 | ReDoS / regex injection | Sink | v1.5 |
| 22 | CRLF / header / email header injection | Sink | v1.5 |
| 23 | Open redirect | Sink | v1.5 |
| 24 | Variable overwrite (`extract`, `parse_str`) | Sink | v1.5 |
| 25 | Type juggling / magic hashes | Input | v1.5 |
| 26 | Reflected XSS in output | Output | v2.0 |
| 27 | Information leakage | Output | v2.0 |
| 28 | Security response headers | Output | v2.0 |
| 29 | Session fixation / cookie tampering | Sink | v2.1 |
| 30 | Virtual patching (IGR rules) | Input + Sink + Output | v2.1 |
| 31 | Brute force / credential stuffing | Behavior | v2.2 |
| 32 | Rate limiting, auto-ban, IP allow/deny | Behavior | v2.2 |

**FR-9 — every new class** is a module (FR-10) with uniform directives,
profile defaults, an attack and benign corpus, fuzzed parsers, a performance
budget and zero overhead when disabled.

## 11. InletGuard Rules (IGR)

Own rule format for virtual patching and fine tuning (engine in v2.1, format
`version 1`). Normative English spec will live in `docs/rules.md`.

```
version 1

rule 10001 "CVE-2025-1234: foo-plugin PHP object injection" {
    match    request.path starts_with "/wp-content/plugins/foo/"
    match    sink.function == "unserialize"
    match    sink.arg[0] is tainted
    action   block
    severity critical
}

rule 10003 "Allow HTML in the post editor" {
    match  request.path == "/wp-admin/post.php" and request.method == "POST"
    action allow xss on post["content"]
}
```

- Stages: `request`, `sink`, `response` (inferred from fields).
- Fields: `request.*`, `get/post/cookie/header/json/args/files[..]`,
  `sink.function`, `sink.arg[N|"name"]`, `sink.file/line`, `response.*`.
- Operators: comparisons, `~` (PCRE), `in`, `in_cidr`, `contains`,
  `starts_with`, `ends_with`; checks `is tainted`, `is sqli/xss`, type checks,
  `breaks sql|shell|path|url|ldap|xpath|html|php`; transforms via `|`.
- Actions: `log`, `block`, `sanitize`, `allow <classes> [on <fields>]`,
  `set_mode`; strictest wins; `allow` never cancels a rule's `block`;
  `dry_run`; global ceiling `inletguard.rules.mode`.
- Files: `inletguard.rules_files` (default `/etc/inletguard/rules.d/*.igr`),
  loaded at startup; broken rules are skipped with
  `file:line:col: error: ...`; unique IDs with reserved ranges.
- Author tooling: `InletGuard\rules_lint()`, `rules_eval()`, CLI
  `inletguard-rules lint|test`.
- Backlog: offline converters from Snuffleupagus / ModSecurity.

Full grammar (EBNF) and semantics: `PRD.ru.md`, section 11.

## 12. Open questions

All original questions are resolved: configurable `sanitize` for SQLi,
configurable log rotation, JSON body in v1.1, learning mode (FR-11), MIT
license, PHP 7.0+ (no 5.x), own rule format IGR, `shm` counters with
optional Redis/Memcached.

## 13. Implementation notes

> Project rule (`AGENTS.md`): take a task → think it through and write a note
> here **and** in `PRD.ru.md` → only then implement. A note records the
> decision, rejected alternatives and the test plan. Numbers refer to
> `TASKS.md`. The Russian notes are more detailed.

### 0.1 Extension skeleton *(written after the fact — the AGENTS.md rule was noticed after implementation)*
- Classic PECL layout (`config.m4`, `php_inletguard.h`, `inletguard.c` in the root); core in `src/core/` (pure C99), integration in `src/zend/`.
- `--enable-inletguard-dev` adds `-Wall -Wextra -Werror` (minus `unused-parameter` and `missing-field-initializers`, produced by Zend macros); not default, so user builds on newer compilers do not break.
- `-std=gnu99` for the extension (PHP 7.x headers use GNU extensions); the core is checked with strict `-std=c99 -pedantic` in the unit build.

### 0.3 C unit test runner *(after the fact)*
- Own ~100-line runner instead of cmocka/Unity: zero dependencies, works with old toolchains.
- Tests self-register via `__attribute__((constructor))`; standalone `tests/unit/Makefile` (no PHP needed), exposed as `make unit` / `make unit-asan`.

### 0.2 `InletGuard\version()` — first TDD cycle
- Single version source `src/core/ig_version.h`; `ig_version()` in the core.
- Namespaced function via `ZEND_NS_NAMED_FE` (available in 7.0–8.4).
- `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX` has an extra `class_name` argument in 7.0–7.1 → first compat wrapper `IG_BEGIN_ARG_WITH_RETURN_TYPE_INFO` in `php_compat.h`.
- Tests first: unit test (red: missing header), phpt for value/format (red: undefined function), Reflection phpt for the signature.
- Result: green on 7.0, 7.1, 7.2, 7.4, 8.1, 8.4 NTS and 8.4 ZTS; the Reflection test uses `getName()` because `__toString()` is deprecated in 7.1–7.4.

### 0.5 / 0.5b CI `ci.yml`
- Official `php:<ver>-cli|zts` images exist for 7.0–8.4 with `phpize` and `php-cgi`; but `php:7.0-cli` has glibc 2.24 and JavaScript actions need node 20+ (glibc ≥ 2.28) → no job-level `container:`; checkout on the host, build via `docker run` with `scripts/ci/phpt.sh` (same script locally).
- `REPORT_EXIT_STATUS=1` (old `run-tests.php` exits 0 on failures).
- Deviations found while running every job locally: `gcc:4.8` uses a v1 manifest → replaced by `centos:7` (GCC 4.8.5, glibc 2.17, vault repos); stock PHP loads extensions with `RTLD_DEEPBIND`, incompatible with ASan → a small `dlopen()` interposer strips the flag, works on 8.1+ images; 7.x ASan moves to 0.5d (PHP built from source); `-Werror` off in ASan mode (PHP 8.1 headers redefine `__SANITIZE_ADDRESS__` under GCC 14); LeakSanitizer off (fails in unprivileged containers), leaks covered by Valgrind.
- Negative check: a deliberate heap overflow fails both the ASan and the Valgrind job (the first trap used a constant `emalloc(4)`, which lands in an 8-byte size class and was not detected — traps must use a dynamic size).

### 0.6 Linters, 0.11 README/CONTRIBUTING, 0.12 Licensing
- clang-format pinned to 18.1.8; Zend tables wrapped in `clang-format off/on`; cppcheck on `src/core`; clang-tidy deferred to phase 1.
- MIT `LICENSE`, `THIRD_PARTY_NOTICES` (empty until libinjection is vendored), `scripts/check-license.sh` (SPDX headers, no GPL/LGPL in `third_party/`), verified to fail on a file without a header.

### Bilingual docs and GitHub repository
- `PRD.ru.md`/`TASKS.ru.md` are authoritative, `PRD.md`/`TASKS.md` are baseline English versions; every change goes into both files of a pair in the same commit, including implementation notes.
- Repository created with `gh repo create` (public), description and topics set with `gh repo edit`.

### 0.5b Branch protection for `main`
**Decision:**
- Require a pull request and passing status checks; **0 required approvals** (single maintainer cannot approve own PRs); no "branch must be up to date" (strict) requirement — avoids rebasing every PR after each merge.
- Only one required check: an aggregate job **`ci-ok`** that `needs` all other jobs and fails if any of them failed or was cancelled. Requiring 31 matrix job names would break as soon as the matrix changes (PRs would wait forever for a check that no longer exists).
- Never use `paths-ignore`/`paths` filters on `ci.yml`: a required check that does not run blocks the PR forever.
- Admins are also subject to the rules (`enforce_admins`), so the process is the same for everyone; force pushes and branch deletion are blocked.

**Rollout:** add `ci-ok` on a branch → open a PR → after `ci-ok` reports green, enable protection via the GitHub API → merge the PR with `gh pr merge` (validates the new flow end to end).

### 0.5a Compatibility layer `php_compat.h`
- The layer is living: wrappers are added by the task that needs them (YAGNI; e.g. the nullable return type wrapper arrives with 5.1 `check_sqli(): ?string`).
- 0.5a delivers the foundation: `scripts/check-compat.sh` fails if `PHP_VERSION_ID`, `PHP_MAJOR/MINOR/RELEASE_VERSION`, `ZEND_MODULE_API_NO` or `ZEND_EXTENSION_API_NO` appear in any `*.c`/`*.h` outside `src/zend/php_compat.h` (excluding `third_party/`); runs in the `lint` job. Wrapper conventions documented in the header: `IG_` prefix, a comment saying what differs since which version, a phpt for each wrapper.
- A grep script rather than a clang-tidy/cppcheck rule: the check is by macro name and file path, needs no PHP headers.
- Tests first: `tests/scripts/test-check-compat.sh` builds temporary trees (clean tree passes; a version check in another file fails and names it; mentions in comments count too; `third_party/` is ignored).
- Outcome: the first CI run failed because the test script was never committed — the `.gitignore` rule `/tests/**/*.sh` (run-tests.php output) silently hid `tests/scripts/*.sh`. Fixed with `!/tests/scripts/`; the branch was rebuilt so the red commit really contains the test. Process lesson: check `git status --ignored` for new files before pushing. `ci-ok` correctly failed after `lint`.

### 0.5c `nightly.yml`
- No fuzz targets, corpora or benchmarks exist yet, so the nightly is a skeleton with **auto-discovery**: later tasks only add files.
- Job `ci`: the full `ci.yml` via `workflow_call`, catching drift of unpinned image tags. `ci.yml` concurrency becomes `${{ github.workflow }}-${{ github.ref }}` so a nightly call and a push to `main` do not cancel each other.
- Job `fuzz`: `tests/fuzz/Makefile` discovers `fuzz_*.c`, builds each with clang `-fsanitize=fuzzer,address,undefined` plus the core sources and runs it for `FUZZ_SECONDS` (300 nightly, configurable on manual runs). Seed corpus in `tests/fuzz/corpus/<target>/`, working corpus kept with `actions/cache`, crashes uploaded as artifacts. With no targets it prints "no fuzz targets yet" and passes.
- Accuracy corpora and benchmarks join in 1.8 and 7.3 together with their runners (YAGNI).
- Tests first: `tests/scripts/test-fuzz-harness.sh` checks the harness itself with temporary targets (no targets → success; safe target → success; buggy target with the crashing input in its seed corpus → failure and a `crash-*` file). Runs in the CI `lint` job.

### 0.5d `release.yml` dry-run
- Build environment: `quay.io/pypa/manylinux2014_{x86_64,aarch64}` (CentOS 7, glibc 2.17, gcc 10), pinned by digest; aarch64 on ARM runners.
- PHP built from source, minimal (`--disable-all`); ZTS flag `--enable-maintainer-zts` on 7.x, `--enable-zts` on 8.x; `CFLAGS=-fcommon` for old PHP with gcc 10.
- PHP versions and tarball SHA256 pinned in `scripts/release/php-versions.txt` and verified before building. Patch level does not change the extension ABI within a branch.
- Built PHP is cached per version × variant × arch × script hash.
- Each build: extension against its PHP → phpt with the same PHP → archive `inletguard-<ver>-php<X.Y>-<nts|zts>-<arch>-linux-gnu.tar.gz` (`.so`, `20-inletguard.ini`, `README.md`, `LICENSE`, `THIRD_PARTY_NOTICES`).
- A final job bundles all archives, a source tarball and `SHA256SUMS` as a 7-day workflow artifact. **No publishing step exists** (comes with the locks in 0.5e).
- Deviations: PECL `package.xml` moves to 7.7 (maintainer metadata, file list); attestations only on real releases (they write to the public Sigstore log); no digest pins for the CI `php:*` matrix (nightly catches drift, EOL tags are frozen, Dependabot cannot bump matrix images) — release build images are pinned.
- `packaging/20-inletguard.ini` is a one-liner for now (full version in 2.22). 7.x ASan follows in a separate PR using the same `build-php.sh` with `-fsanitize=address`.
- Tests first: `tests/scripts/test-release-scripts.sh` checks `version.sh`, the versions file (exactly branches 7.0–8.4, matching versions, 64-char SHA256) and `package.sh` (archive name and exact file set on a dummy `.so`). The full PHP build is verified locally in a manylinux container (7.0 and 8.4, NTS and ZTS) and then by the workflow.

### ASan for PHP 7.x (follow-up of 0.5 / 0.5d)
- Verified: PHP 7.0/7.4 `DL_LOAD` skips `RTLD_DEEPBIND` when `__SANITIZE_ADDRESS__` is defined, so an ASan-built PHP loads extensions without the shim or `LD_PRELOAD`. Both build with the runner toolchain (gcc 13) via `build-php.sh`. UBSan reports UB inside PHP 7.x itself (`zend_alloc.c`, `zend_hash.c`), so PHP gets ASan only.
- Job `phpt-asan-php7` (7.0, 7.4) on the runner host: cached ASan PHP; the extension is built with `-fsanitize=address,undefined` (UBSan checks only our code). `phpt.sh --asan-native` = sanitizer build without shim/preload, PHP from `PATH`. `ci-ok` needs the new job.
- Check: phpt green locally; the heap-overflow trap fails the run on 7.0 and 7.4.
- Outcome: on 7.0 the trap first went **undetected** although a direct `php` run caught it — `run-tests.php` of PHP 7.0–7.3 forces `USE_ZEND_ALLOC=1` for test processes unless `-m` is given, and ASan cannot see overflows inside Zend allocator chunks. In ASan modes `phpt.sh` now patches its copy of `run-tests.php` to keep `USE_ZEND_ALLOC=0`; the trap then fires on 7.0 and 7.4, regular `--asan` on 8.4 stays green. Lesson: verify every sanitizer job with a trap on every version.

### 0.5e Publishing locks
- Policy lives in `scripts/release/check-policy.sh <ref> <releases_enabled>` (testable, unlike YAML expressions). Publishing requires **all** of: a SemVer tag ref `refs/tags/vX.Y.Z[-pre]`; major ≥ 1 (`v0.*` tags are rejected with an error); repository variable `RELEASES_ENABLED=true` (enabled in 7.8). Non-tag refs → "dry run, nothing to publish", success.
- `release.yml`: job `policy` runs first (a rejected tag does not spend 40 builds); job `publish` (`needs: [policy, bundle]`) runs only when allowed, in the **`release` environment** with required maintainer approval and a `v*` tag deployment policy.
- `publish`: build attestations for all bundle files (only this job gets `id-token`/`attestations`/`contents: write`), then a **draft** release via `gh release create --draft --verify-tag`; the maintainer publishes the draft manually; PECL/Packagist manually in 7.8. Notes: `--generate-notes` until CHANGELOG extraction (0.5f).
- Testing without public test tags: manual runs accept `simulate_ref` for the policy check only; `publish` requires a real tag in `github.ref`. Replaces the planned `v0.0.0-test` tag, which would be public.
- Outcome (manual runs on `main`): `simulate_ref=refs/tags/v0.0.0-test` → `policy` fails "before v1.0.0"; `refs/tags/v1.0.0` → "RELEASES_ENABLED is not 'true'"; builds and `publish` skipped. Manual runs on the same ref share a concurrency group and cancel each other — run them one at a time.
- Tests first: `tests/scripts/test-release-policy.sh` (allowed: `v1.0.0`, `v1.2.3-rc.1` when enabled; rejected: `v0.9.0`, disabled, non-SemVer tags; branch refs → dry run).

### 0.5f Version consistency
- `scripts/release/check-version.sh <tag>` checks for `vX.Y.Z[-pre]`: `ig_version.h` equals the tag version; no `-dev` suffix; `CHANGELOG.md` has a non-empty `## [X.Y.Z[-pre]] - YYYY-MM-DD` section (Keep a Changelog); `package.xml` `<release>` matches if the file exists (it arrives in 7.7, so it is optional now). Reports all problems, exit 1.
- `scripts/release/changelog-notes.sh <version>` prints the CHANGELOG section body, used as release notes instead of `--generate-notes`.
- `release.yml`: version check in the `policy` job for any tag (before builds); `publish` takes notes from CHANGELOG. `CHANGELOG.md` created with `## [Unreleased]`.
- Tests first: `tests/scripts/test-check-version.sh` with temporary trees (consistent → ok; tag ≠ header, `-dev`, missing/empty CHANGELOG section, mismatching `package.xml` → fail; missing `package.xml` → not checked; notes extraction returns exactly the section body).

### 0.5g Artifact smoke tests
- Verified sources: `ondrej/php` PPA on Ubuntu 24.04 (7.0–8.4), Remi on AlmaLinux 8 (7.2–8.4) and 9 (7.4–8.4), official `php:*-cli|zts` images (all branches, both arches).
- `scripts/release/smoke.sh <php> <archive-dir>`: asks that PHP for branch, ZTS and arch, picks exactly one matching archive, checks that `php -n -d extension=<.so>` loads it with the right `InletGuard\version()`, and that the packaged `20-inletguard.ini` works too. Missing/ambiguous archive or wrong version → failure.
- `smoke` jobs in `release.yml` after `bundle`: official images (cli + zts, both arches), `debian12-sury` (10 branches), `alma8-remi`, `alma9-remi`. CentOS 7 is covered by the phpt run inside `manylinux2014` during every build. The out-of-the-box scenario (FR-13.7) joins in 7.5a.
- Waiting on runs now uses `gh run view --json conclusion` instead of the `gh run watch` exit code (unreliable for finished runs; caused a false alarm on PR #7).
- Local run on the dry-run bundle: official x86_64 images 20/20, Debian 12 + `packages.sury.org` 10/10, AlmaLinux 8 + Remi 8/8, AlmaLinux 9 + Remi 6/6. The Launchpad `ondrej/php` PPA returned 503 twice and announces its move to `packages.sury.org`, so Debian 12 + `packages.sury.org` (same builds, canonical source) replaces Ubuntu + PPA, with apt retries and CLI-only installs.
- Tests first: `tests/scripts/test-smoke.sh` with a stub `php` and fake archives (correct archive selected; none/two matching → failure; wrong version → failure; packaged ini path exercised).

### 0.7 Vendoring libinjection
- Active upstream: `github.com/libinjection/libinjection`, **v4.0.0** (March 2026, commit `2117822`); `client9/libinjection` is stale since 2023. BSD-3-Clause.
- v4.0 returns `injection_result_t` (`FALSE`/`TRUE`/`ERROR`) and **no longer calls `abort()`** on parser errors — essential for NFR-2/3; `ERROR` will map to `internal_error`/`fail_mode`.
- 9 files from `src/` vendored **unmodified** into `third_party/libinjection/` with `COPYING` and `README.inletguard.md` (upstream, tag, commit, SHA256 per file). `scripts/vendor-libinjection.sh <tag>` re-vendors reproducibly; `scripts/check-vendor.sh` (in `lint`) verifies the checksums.
- Builds with GCC 4.8.5; GCC 13 reports 2 upstream `-Wenum-int-mismatch` warnings → third-party code gets separate, lenient flags (`PHP_ADD_SOURCES_X`, separate unit/fuzz rules); our code stays `-Werror`.
- `THIRD_PARTY_NOTICES` entry. API wrappers are tasks 1.1–1.2.
- Tests first: a unit test that the library is linked with the expected version (`libinjection_version()`), and `check-vendor.sh` must fail on a tampered copy.

### Smoke tests vs. flaky external repositories (follow-up of 0.5g)
- On PR #9 `smoke almalinux 9 + remi` failed on EPEL mirrors ("All mirrors were tried"), earlier Launchpad returned 503s — infrastructure, not product failures.
- `scripts/release/retry.sh <attempts> <delay> <cmd...>` retries a command and keeps the last exit code. All network steps of `smoke-remi.sh` and `smoke-debian-sury.sh` go through `retry.sh 5 30`; dnf also gets `retries=10`, `timeout=60`. The extension load checks themselves are never retried.
- Tests first: `tests/scripts/test-retry.sh` (fails twice then succeeds with 3 attempts → success; always failing → failure after exactly N attempts; last exit code preserved).

### 0.8 [SPIKE] Input interception via `sapi_register_input_filter`
- Goal: confirm or refute the FR-1 mechanism before production code. Throw-away prototype outside the repo; outcome in `docs/adr/0001-input-interception.md`.
- Questions (7.0 and 8.4, CGI, ext/filter built in): (1) chaining with ext/filter keeps `filter_input()`/`filter_has_var()` working; (2) do we see values before ext/filter and what does `filter_input()` return if we change a value; (3) which sources pass through the filter (GET, urlencoded POST, multipart text fields, COOKIE, `$_SERVER`/headers, `$_ENV`; multipart files?); (4) what `var` looks like for arrays (`a[b][c]`); (5) are server variables filtered with `auto_globals_jit=1` when the script never touches `$_SERVER`; (6) does the filter run before or after our RINIT; (7) when is `input_filter_init` called.
- Method: a prototype extension logs every call (arg, var, value length, before/after RINIT) and chains to the previous filter; run with `php-cgi` using `QUERY_STRING`, urlencoded and multipart bodies, `HTTP_COOKIE` and headers; the test script calls `filter_input()`.
- Outcome (identical on 7.0 and 8.4, CGI and FPM) → ADR-001: chaining with ext/filter works; we run first and our changed value is what both `$_GET` and `filter_input()` see (sanitize in the filter). GET, COOKIE, urlencoded POST and multipart text fields pass; files do not; `var` is the full bracketed name. **Headers and `REQUEST_URI` are unreliable through the filter** (never in CGI; in FPM only when `$_SERVER` is materialized, i.e. after RINIT) → read them in RINIT from a force-materialized `$_SERVER`, ignore `PARSE_SERVER`/`PARSE_ENV` in the filter. Order: `input_filter_init` → filter calls → RINIT, `input_filter_init` runs per request → reset request state there. Declare the callback with `SAPI_INPUT_FILTER_FUNC()` (`char *` vs `const char *var`). Blocking is decided in RINIT (spike 0.9).

### 0.9 [SPIKE] Blocking mechanism
- Goal: once RINIT decides to block (ADR-001), send the block response and run **no line** of the user script — the same way under CGI, FPM, Apache mod_php (and LSAPI if feasible), on 7.0 and 8.4. Outcome: ADR-002.
- Candidates: (1) RINIT returns `FAILURE`; (2) send the response in RINIT and `zend_bailout()`; (3) wrap `zend_compile_file`: on the first compile of the request (main script or `auto_prepend_file`) send the response and `zend_bailout()` — like `exit()` on 7.x; (4) wrap `zend_execute_ex` and skip the main script.
- Criteria: status 403 with our body; the script did not run (no marker file); clean request end (shutdown, no stray errors, the FPM worker serves the next request); **behaviour with OPcache** (risk: OPcache wraps `zend_compile_file` and may skip our hook for cached scripts); cost for unblocked requests (`zend_execute_ex` disables fast VM calls for every function); ZTS safety (hook pointers cannot be switched per request under ZTS).
- Method: a prototype extension with a mode switch, blocking on `GET block=1`; the script creates a marker file; runs under `php-cgi`, `php-fpm` + `cgi-fcgi` (two requests in one worker) and `php:*-apache` + `curl`, each with and without OPcache.
- Outcome (identical on 7.0 and 8.4) → ADR-002: RINIT `FAILURE` sends no block page and the FPM worker exits; response + `zend_bailout()` in RINIT looks right from outside but **the FPM log shows the worker dies after every blocked request** (a DoS amplifier) — rejected; a `zend_compile_file` hook from MINIT is **bypassed by OPcache** — rejected. **Accepted:** a `zend_compile_file` wrapper installed once per process in the first RINIT (after OPcache, so ours is outermost; mutex under ZTS); on the first compile of a blocked request: status, header, body via `php_output_write()`, then `zend_bailout()` (the 7.x `exit()` path). Works on CGI, FPM, Apache, with and without OPcache, on ZTS; no worker deaths. `zend_execute_ex` works too but slows every call — fallback only. Lessons: never keep pointers into `environ` (FPM rewrites it; the first spike run was invalid because of it); read logs, not just responses. LSAPI verified later on the stand (7.5).

### 0.10 [SPIKE] Hooking internal functions and methods
- Facts: official images build `PDO`, `pdo_sqlite`, `sqlite3`, `mysqlnd` statically; `mysqli` is a shared extension installed separately, so it may load **after** us.
- Questions (7.0 and 8.4): (1) replacing `zend_internal_function.handler` for functions and methods (class `function_table`), calling the original; (2) load order — is a later-loaded shared extension visible in our MINIT, or do we install hooks once per process in the first RINIT (as in ADR-002), or use `ZEND_MOD_OPTIONAL` dependencies; (3) reading arguments via `ZEND_CALL_ARG()`, blocking by throwing an exception without calling the original; (4) aliases (`mysqli::query` and `mysqli_query` are separate entries with one C handler); (5) Xdebug 2.x (7.0) / 3.x (8.4) compatibility; (6) OPcache + JIT on 8.4 do not cache the handler pointer; (7) PHP 8.4 frameless functions (`ZEND_FRAMELESS_ICALL`) bypass `handler` — list them for the future sink registry (8.3).
- Outcome (7.0 and 8.4) → ADR-003: handler replacement works for functions and methods; arguments via `ZEND_CALL_ARG`; throwing without calling the original gives a catchable exception at the caller's line; aliases are separate entries. **Load order:** in our MINIT a later-loaded shared extension's functions exist but its **classes do not** (`mysqli::query` missing) → install in the first RINIT (like ADR-002). OPcache and 8.4 tracing JIT do not bypass hooks (5000/5000); Xdebug 2.6.1 (built from source, old PECL clients no longer get 2.x) and 3.5.3 are compatible. **PHP 8.4 frameless functions** (`preg_match`, `preg_replace`, `str_replace`, `strpos`, `substr`, …) bypass `handler` via `ZEND_FRAMELESS_ICALL` even without OPcache; clearing `frameless_function_infos` in MINIT restores interception (needed for 12.5 and string sinks). ZTS requirement (derived, not exercised with real threads): idempotent installation in every thread's first RINIT.
- Method: a prototype swaps handlers, logs calls and the first argument, throws on a `BLOCKME` marker; real PDO/SQLite3 (in-memory sqlite), `mysqli_query` without a server; mysqli as a shared extension loaded after the prototype; runs with/without Xdebug and with/without OPcache+JIT.

### 0.13 [SPIKE] Pool / virtual host configuration
- Goal: decide which directives may be set per pool / per vhost (FR-10.5) and which only in the global `php.ini`. Outcome: ADR-009.
- Questions (7.0 and 8.4): (1) FPM `php_admin_value` for a `PHP_INI_SYSTEM` directive — visible in MINIT (master) or only in the worker (RINIT), and at which `ZEND_INI_STAGE_*`; (2) FPM `php_value`, `.user.ini`, `ini_set()`; (3) Apache mod_php `php_admin_value` in `<VirtualHost>` — when applied, and does it persist across requests to different hosts served by one process; `php_value` in `.htaccess`; (4) consequence for hooks installed in the first RINIT (ADR-002/003): a directive read there would carry the **first served host's** value under Apache — can the global value (`orig_value`) be read instead?
- Outcome → ADR-009: FPM pool values (`php_admin_value`/`php_value`) are applied when the worker starts (stage ACTIVATE) and become its base value — **not visible in the master's MINIT**. Apache vhost `php_admin_value` is applied per request before RINIT (`modified=1`, `orig_value` = global) and restored afterwards; vhost `php_value`, `.user.ini`, `.htaccess` are ignored for `PHP_INI_SYSTEM`. **Hole in PHP 7.0 and 7.1** (fixed in 7.2): an FPM pool `php_value` makes a `PHP_INI_SYSTEM` directive changeable with `ini_set()` — closed by an `OnUpdate` stage guard (refuse RUNTIME/HTACCESS), verified on 7.0, 7.1, 8.4. Apache + 8.4 + OPcache: userland `ini_get()` returned **another host's** value (OPcache folds `ini_get()` of SYSTEM directives at compile time) → `InletGuard\config()` reads in C. Decision: all directives SYSTEM + stage guard; **startup** directives from global `php.ini` only (MINIT value remembered, pool/vhost override → one `config_warning` per process); **runtime** directives read in C per request, settable per pool/vhost.
- Method: a prototype with a `PHP_INI_SYSTEM` directive `igcfg.mode` and an `OnUpdate` handler logging the stage; values logged in MINIT and RINIT (current and `orig_value`); the script prints `ini_get()` and the `ini_set()` result. FPM: two pools (`php_admin_value`, `php_value`) and `.user.ini`; Apache: two vhosts (one with `php_admin_value`) and `.htaccess`, requests alternating in one process.

### 0.14 Module build flags
- One option `./configure --with-inletguard-modules=LIST` with the same syntax as the `inletguard.modules` directive: `all` (default), `sqli,xss`, `all,-xss`, `none` (PHP API only). **Deviation:** no separate `--disable-inletguard-<m>` flags — one syntax for build and ini; `all,-m` covers it.
- Unknown names, an empty list or excluding an unknown module make `configure` fail with the list of known modules (a typo must not silently build an extension without protection).
- Output: `IG_MODULE_<M>` 1/0 per known module and `IG_COMPILED_MODULES` in `config.h`; module sources are built only when enabled (none exist yet). The list of known modules lives in `config.m4` for now and moves into the module registry in 2.8. `phpinfo()` shows "Compiled-in modules".
- Tests first: phpt `004-compiled-modules.phpt` (expected list from `IG_EXPECT_MODULES`, default `sqli, xss`); `tests/scripts/test-configure-modules.sh` (all/list/exclusion/none accepted; unknown names, empty value, excluding unknown → configure fails); CI job `phpt-minimal-modules` building with `sqli` only.


### 0.15 PHP 8.5 support
- **Facts (verified 2026-10-22):**
  - PHP 8.5.12 (21 Oct 2026); php.net publishes the `.tar.xz` SHA256.
  - Official `php:8.5-cli|zts` images exist for amd64 and arm64.
  - Packages: `packages.sury.org` for Debian 12 (`php8.5-cli/cgi/dev`) and Remi for EL8 and EL9 (`php85`).
  - Without code changes:
    - phpt NTS/ZTS pass with `-Werror`;
    - ASan with the dlopen shim catches a planted overflow, as on 8.4;
    - Valgrind is clean and `test-configure-modules.sh` passes;
    - PHP 8.5 builds from source in manylinux2014 x86_64, and the extension builds, passes phpt and is packaged.
- **Decision:**
  - **Matrices.** 8.5 is added to every matrix: phpt (11 branches × NTS/ZTS), release builds (44, `bundle` checks 44) and smoke (official x86_64/aarch64, Debian 12 + sury, AlmaLinux 8/9 + Remi).
  - **Newest-version jobs.** ASan with the shim, Valgrind, aarch64 and minimal modules move from 8.4 to 8.5. ASan keeps 8.1, the oldest version where the shim works, and 8.4 stays covered by the main matrix.
  - **Single source for the branch list:** `scripts/release/php-versions.txt`. The new `scripts/check-php-matrix.sh` (job `lint`) checks it against:
    - the `phpt` matrix in `ci.yml` and the `build` matrix in `release.yml`;
    - the archive count in `bundle` (branches × 4);
    - the smoke branch lists: all branches for official images and sury; for Remi, a contiguous tail of the list ending at the newest branch;
    - the newest-version jobs in `ci.yml`;
    - the version line in `README.md` and the "7.0–X" ranges in README, CONTRIBUTING, the PR template and `php_compat.h`.

    Adding 8.6 means one line in the versions file plus whatever the check points out.
  - **Frameless functions.** The list may have changed in 8.5, but ADR-003 point 3 does not depend on it (`frameless_function_infos` is cleared for every hooked function that has them). This is checked when sinks are implemented (8.3, 12.5).
  - **8.4 in the versions file** is not bumped: a patch release keeps the ABI.
  - PRD and TASKS statements about the supported range are updated to 7.0–8.5. Historical notes of finished tasks are left unchanged.
- **Tests first:**
  - `test-release-scripts.sh` expects branches 7.0–8.5.
  - `tests/scripts/test-check-php-matrix.sh` uses temporary trees: a consistent tree passes, and each drift is reported with a clear message — a branch missing from the phpt or build matrix, a wrong archive count, a smoke list without a branch, Remi without the newest branch, README without a branch, a stale "7.0–8.4" range in README, a newest-version job on an old branch.
- **Outcome (PR #16):** CI green on 7.0–8.5. Release dry run: 4 builds of 8.5, a 44-archive bundle, and all smoke jobs (official x86_64/aarch64, sury, Remi 8/9) passed on 8.5.

### 1.1 `ig_detect_sqli` wrapper over libinjection
- **Facts (vendored 4.0.0):**
  - The fingerprint buffer is 8 bytes: up to 5 tokens plus NUL.
  - `libinjection_sqli` never returns `ERROR` (only the XSS HTML5 parser does); the mapping is still needed, defensively and for future versions.
  - Classic payloads give stable fingerprints (`' OR 1=1--` → `s&1c`, `1 UNION SELECT …` → `1UEnk`, `1; DROP TABLE users` → `1;Tnn`).
  - `O'Reilly`, `Rock 'n' roll`, `I'm here`, Russian text and "drop the table and union with friends" are clean.
  - An embedded NUL does not hide an attack, because the length is explicit.
- **Decision:**
  - `src/core/ig_detect.h`:
    - `ig_verdict_t` (`IG_CLEAN` 0, `IG_ATTACK` 1, `IG_ERROR` -1);
    - `ig_result_t { verdict; fingerprint[IG_FINGERPRINT_SIZE = 8] }`;
    - `ig_detect_sqli(s, len, res)` returns the verdict it stores.

    `truncated` (1.6) and the detector id (1.3) come with their own tasks.
  - **Contract:**
    - `res` is always filled;
    - the fingerprint is always NUL-terminated and non-empty only for `IG_ATTACK`, and it is cleared when `res` is reused;
    - `res == NULL` → `IG_ERROR`, nothing is written;
    - `s == NULL` with `len > 0` → `IG_ERROR`;
    - `s == NULL` with `len == 0` → clean.
  - **Adapter `src/core/ig_libinjection.{h,c}`:** `ig_li_verdict()` maps FALSE → CLEAN, TRUE → ATTACK, and ERROR or **any unknown value** → ERROR. Unknown is never "clean": `internal_error` and `fail_mode` decide in phase 4. XSS (1.2) shares the adapter.
  - **No global state:** the libinjection state lives on the stack, so the function is ZTS-safe.
  - **Fuzz target `fuzz_detect_sqli.c`** checks the invariants: verdict in {-1, 0, 1}, fingerprint NUL-terminated within the buffer and non-empty iff ATTACK.
- **Tests first:**
  - 10 attacks with exact fingerprints (updated deliberately together with libinjection);
  - 13 benign strings → clean with an empty fingerprint;
  - an embedded NUL, NULL arguments, fingerprint reset on reuse;
  - the adapter: FALSE / TRUE / ERROR / unknown value.


### 0.6 (continued) `clang-tidy`
- **Facts:**
  - `clang-tidy` 18.1.8 comes from PyPI, like the pinned `clang-format`.
  - On the current `src/core`, the broad set (`bugprone`, `cert`, `clang-analyzer`, `misc`, `performance`, `portability`, `readability`) reports only `misc-include-cleaner` and `readability-identifier-length`.
  - On planted bugs it catches an uninitialized value, a double `free` and `signed char` misuse. It misses `sizeof` of a pointer in `memcpy` and dereferencing an unchecked `malloc`; those are left to ASan, fuzzing and review.
- **Decision:**
  - **`.clang-tidy`** with those groups, `WarningsAsErrors: '*'`, and `HeaderFilterRegex` limited to `src/core/`.
  - **Disabled, with reasons:**
    - `misc-include-cleaner` (no IWYU mappings);
    - `readability-identifier-length` (idiomatic short C names);
    - `bugprone-easily-swappable-parameters` (`(const char *, size_t)` APIs);
    - `…insecureAPI.DeprecatedOrUnsafeBufferHandling` (Annex K, absent in glibc);
    - `readability-magic-numbers` (decoder tables in 1.4/1.5).
  - **`scripts/check-tidy.sh [root]`** runs `$CLANG_TIDY` (default `clang-tidy`) over `src/core/*.c` with the core build flags. A missing tool is an error.
  - **CI, `lint`:** `pipx install clang-tidy==18.1.8`, the script and its test.
  - **`src/zend`/`inletguard.c`** need PHP headers and will be added later in a PHP container.
- **Tests first** (`tests/scripts/test-check-tidy.sh`, temporary trees with the real `.clang-tidy`):
  - the current `src/core` passes;
  - planted bugs fail and name the file: uninitialized value, double free, `signed char`, a bug in a `src/core` header;
  - `third_party/` is ignored;
  - a missing `clang-tidy` fails.

### 1.2 `ig_detect_xss`
- **Facts (vendored 4.0.0):**
  - `libinjection_xss` catches `<script>`, `onerror=`, `<svg onload>`, `href="javascript:…"`, mixed case, breaking out of an attribute (`"><script>`), `style=url(javascript:…)` and an attack after an embedded NUL.
  - `<b>`, `<p class>`, `<a href="https://…">`, `<img src alt>`, `2 < 3`, `I <3 you` and Russian text with `<b>` are clean.
  - It does **not** catch a bare `javascript:alert(1)`, which is XSS when the application puts the value into `href`/`src`.
  - The HTML5 parser can return `ERROR`, and `libinjection_xss` passes it on.
- **Decision:**
  - `ig_detect_xss(s, len, res)` has the same contract as `ig_detect_sqli`.
  - XSS has no libinjection fingerprint, so the field names **what fired**: `html` (libinjection) or `url` (our scheme check). This helps triage false positives, and the rule "non-empty iff ATTACK" still holds.
  - Order: libinjection first; if it is not an attack, the scheme check runs. A scheme match beats a libinjection `ERROR` (evidence of an attack wins); otherwise `ERROR` stays `ERROR`.
  - **Scheme check** (WHATWG URL parsing as browsers do it):
    1. skip leading C0 controls and spaces (≤ 0x20);
    2. ignore ASCII tab/LF/CR anywhere in the scheme;
    3. compare case-insensitively with `javascript:`, `vbscript:`, or `data:` followed by `text/html`, `image/svg+xml` or `application/xhtml+xml`.
  - **False positives:** "JavaScript: The Good Parts" in a text field must stay clean, so `javascript:`/`vbscript:` count only when the rest contains a code character `(`, `` ` `` or `=`. Attack forms always have one: `alert(1)`, ``alert`1` ``, `location=…`, `throw onerror=…`. For `data:` the MIME type is enough.
  - HTML entities (`&#106;avascript:`) and URL encoding are handled by the normalizers (1.4/1.5), not here.
- **Tests first** (`tests/unit/test_detect_xss.c`):
  - HTML attacks → `html`;
  - scheme attacks → `url` (case, leading spaces/controls, tab/newline inside the scheme, `data:text/html`, `data:image/svg+xml`);
  - benign HTML and text, `JavaScript: The Good Parts`, `data:image/png`, `https://…`, `javascript` without a colon → clean;
  - embedded NUL, NULL arguments, reuse of `res`.
  - The fuzz target covers both detectors.

