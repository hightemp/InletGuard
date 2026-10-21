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

> **Supported PHP: 7.0 – 8.4**, NTS and ZTS, Linux. The whole 7.x branch is
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
  `libinjection_xss`; a common detector interface for future classes.
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
| NFR-4 Compatibility | PHP 7.0–8.4 × NTS/ZTS; FPM, CGI, mod_php, CLI, LSAPI; ext/filter; x86_64, aarch64; version checks only in `php_compat.h` |
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
across 7.0–8.4; C bugs crashing sites; aging images for EOL PHP versions;
overhead of many hooks; SSRF bypasses (DNS rebinding, URL parser
differentials). Mitigations: monitor/learn, corpora, fuzzing, sanitizers,
`php_compat.h`, CI matrix, performance budgets — see `PRD.ru.md` section 8.

## 9. Testing strategy (TDD)

Red → green → refactor for every task. Levels: C unit tests (core), `.phpt`
integration tests (same tests for all PHP versions), corpora (accuracy),
ASan/UBSan and Valgrind, libFuzzer, performance budgets.

**Definition of done:** tests written before code, green on PHP 7.0–8.4 ×
NTS/ZTS, ASan clean, no compiler warnings.

### 9.1 CI (GitHub Actions)
Built in phase 0, before any production code.

| Workflow | Trigger | Content |
|---|---|---|
| `ci.yml` | push, PR | lint, license/SPDX, unit tests, phpt matrix 7.0–8.4 × NTS/ZTS, ASan, Valgrind, old toolchain, aarch64 |
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

