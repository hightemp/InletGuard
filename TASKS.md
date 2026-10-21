# TASKS: InletGuard

> **Baseline English version.** The authoritative plan with full task
> descriptions is [`TASKS.ru.md`](TASKS.ru.md) (Russian). Task IDs and
> statuses here mirror it; if they disagree, the Russian version wins.

> **Strict TDD.** For every **[TDD]** task: 🔴 write the test first and see
> it fail → 🟢 minimal implementation → 🔵 refactor with green tests + ASan.
> **[SPIKE]** — throw-away prototype, outcome recorded as an ADR in
> `docs/adr/`. **[INFRA]** — build, CI, docs.
>
> **Definition of done:** test written before code (red → green visible in
> history); green on PHP 7.0–8.4 × NTS/ZTS; ASan/UBSan clean; no warnings
> with `-Werror`; everything in English.
>
> **Modularity:** every attack class is a module (PRD FR-10) that can be
> disabled at build time, at startup, per pool/vhost and by a rule; every
> module task includes a "disabled → no effect" test.
>
> **Before coding** a task, write an implementation note in `PRD.ru.md`
> and `PRD.md`, section 13 (see `AGENTS.md`). Every change to PRD/TASKS
> goes into both language versions.

---

## Phase 0. Skeleton, test infrastructure, spikes

- [x] **0.1 [INFRA]** Extension skeleton (`config.m4`, module entry, `src/core`, `src/zend`, `tests`).
- [x] **0.2 [TDD]** First phpt: extension loaded, `InletGuard\version()`.
- [x] **0.3 [INFRA]** Dependency-free C unit test runner (`make unit`).
- [x] **0.4 [INFRA]** ASan/UBSan builds, Valgrind phpt runs.
- [x] **0.5 [INFRA]** `ci.yml`: phpt matrix 7.0–8.4 × NTS/ZTS, ASan, Valgrind, old toolchain, aarch64 (7.x ASan moved to 0.5d).
- [x] **0.5a [TDD]** Compatibility layer `src/zend/php_compat.h` (the only place for `PHP_VERSION_ID`): foundation + CI check; wrappers come with the tasks that need them.
- [x] **0.5b [INFRA]** CI hygiene: SHA-pinned actions, Dependabot, PR template, `main` protection (PR + required `ci-ok` check). Digest-pinned images moved to 0.5d.
- [x] **0.5c [INFRA]** `nightly.yml`: scheduled full CI + auto-discovering fuzz harness (tested with temporary targets); corpora and benchmarks join in 1.8 and 7.3.
- [x] **0.5d [INFRA]** `release.yml` in dry-run: 40 `.so` builds on glibc 2.17 against verified PHP sources, phpt per build, archives, source tarball, checksums. PECL package → 7.7, attestations → 0.5e, 7.x ASan → follow-up PR.
- [x] **0.5e [INFRA]** Publishing locks: tested `check-policy.sh`, `policy` job before builds, `publish` job (attestations + draft release) only for an allowed real tag in the `release` environment (owner approval, `v*` tags only); `simulate_ref` instead of a public test tag.
- [x] **0.5f [INFRA]** Version consistency check (tag = `ig_version.h` = `CHANGELOG.md` section, no `-dev`, `package.xml` if present) in the `policy` job; release notes from CHANGELOG; `CHANGELOG.md` added.
- [x] **0.5g [INFRA]** Artifact smoke tests: official images (both arches, NTS/ZTS), Debian 12 + packages.sury.org, AlmaLinux 8/9 + Remi; CentOS 7 via phpt in manylinux2014. FR-13.7 scenario in 7.5a.
- [ ] **0.6 [INFRA]** clang-format, cppcheck, clang-tidy, `-Werror`. *Partially done (clang-tidy pending).*
- [x] **0.7 [INFRA]** Vendor libinjection into `third_party/`: v4.0.0 (no `abort()` on parser errors), unmodified with SHA256 record, `check-vendor.sh` in CI, lenient flags for vendored code, full license text in `THIRD_PARTY_NOTICES`.
- [x] **0.8 [SPIKE]** Input interception via `sapi_register_input_filter` chained with ext/filter → ADR-001 (GET/POST/COOKIE via the filter; headers and URI from `$_SERVER` in RINIT; request state reset in `input_filter_init`).
- [x] **0.9 [SPIKE]** Blocking mechanism → ADR-002: `zend_compile_file` wrapper installed in the first RINIT (after OPcache) + `zend_bailout()` on the first compile of a blocked request; RINIT variants kill FPM workers, a MINIT hook is bypassed by OPcache.
- [ ] **0.10 [SPIKE]** Hooking internal functions and methods, Xdebug compatibility → ADR-003.
- [x] **0.11 [INFRA]** `README.md`, `CONTRIBUTING.md`.
- [x] **0.12 [INFRA]** Licensing: MIT `LICENSE`, `THIRD_PARTY_NOTICES`, SPDX headers, CI check.
- [ ] **0.13 [SPIKE]** Pool/vhost-level configuration visibility in MINIT vs RINIT → ADR-009.
- [ ] **0.14 [INFRA]** Build flags for modules (`--with-inletguard-modules`, `--disable-inletguard-<m>`).

## Phase 1. Detection core (pure C, no Zend)

- [ ] **1.1 [TDD]** `ig_detect_sqli()` wrapper over libinjection, fingerprint in result.
- [ ] **1.2 [TDD]** `ig_detect_xss()` wrapper.
- [ ] **1.3 [TDD]** Common detector interface and registry.
- [ ] **1.4 [TDD]** Normalizer: repeated URL decoding, NUL removal, `%u` sequences.
- [ ] **1.5 [TDD]** HTML entity decoding for XSS.
- [ ] **1.6 [TDD]** Length limit with `truncated` flag.
- [ ] **1.7 [TDD]** Glob matcher for exclusions.
- [ ] **1.8 [TDD]** Corpus runner with accuracy thresholds (NFR-5).
- [ ] **1.9 [TDD]** False positive regression process.

## Phase 2. Configuration and modularity

- [ ] **2.1 [TDD]** Register ini directives as `PHP_INI_SYSTEM`.
- [ ] **2.2 [TDD]** `ini_set()` cannot change protection settings.
- [ ] **2.3 [TDD]** Mode parsing and inheritance.
- [ ] **2.4 [TDD]** List parsing (sources, headers, `source:name` exclusions).
- [ ] **2.5 [TDD]** Invalid values → default + `config_warning`.
- [ ] **2.6 [TDD]** `sqli.sink.mode=sanitize` treated as `block`.
- [ ] **2.7 [TDD]** `phpinfo()` section.
- [ ] **2.8 [TDD]** Module registry (module descriptors, profile defaults).
- [ ] **2.9 [TDD]** Uniform per-module directives generated from descriptors.
- [ ] **2.10 [TDD]** `inletguard.modules` (load-time module selection, no hooks for unloaded modules).
- [ ] **2.11 [TDD]** Profiles `off|minimal|balanced|strict|paranoid`.
- [ ] **2.12 [TDD]** Configuration source precedence, pool/vhost values.
- [ ] **2.13 [TDD]** Sensitivity `<m>.level` 1–4.
- [ ] **2.14 [TDD]** Unknown key detection ("did you mean"), `config_strict`.
- [ ] **2.15 [TDD]** Introspection: value sources, secret masking, `InletGuard\config()`, `modules()`.
- [ ] **2.16 [TDD]** `allow_user_tightening` + `InletGuard\tighten()`.
- [ ] **2.17 [TDD]** Deprecated directive aliases.
- [ ] **2.18 [INFRA]** `inletguard-config check` CLI and example ini files.
- [ ] **2.19 [TDD]** Zero-config: no `inletguard.*` directives → `balanced` profile.
- [ ] **2.20 [TDD]** Per-source mode `<m>.input.<source>.mode` (XSS: GET block, POST monitor).
- [ ] **2.21 [TDD]** One-time "running with built-in defaults" message.
- [ ] **2.22 [INFRA]** Packaged `20-inletguard.ini`.

## Phase 3. Logger

- [ ] **3.1 [TDD]** JSON serializer with safe escaping and UTF-8 repair.
- [ ] **3.2 [TDD]** Event structure and serialization, payload truncation.
- [ ] **3.3 [TDD]** `log_payload=0`.
- [ ] **3.4 [TDD]** Atomic append to `log_file`.
- [ ] **3.5 [TDD]** Fallback to `error_log`.
- [ ] **3.6 [TDD]** Unwritable log path never breaks the request.
- [ ] **3.7 [TDD]** `request_id` generation.
- [ ] **3.8 [TDD]** `log_level`.
- [ ] **3.9 [TDD]** `log_rotate=external`: reopen on inode change.
- [ ] **3.10 [TDD]** Concurrent writers produce valid JSON lines.
- [ ] **3.11 [TDD]** Built-in rotation `size|daily|hourly` with cross-process locking.
- [ ] **3.12 [TDD]** Flood protection: rate limit and deduplication.
- [ ] **3.13 [TDD]** Privacy: excluded fields, IP anonymization.
- [ ] **3.14 [TDD]** `log_format=logfmt`.
- [ ] **3.15 [TDD]** Per-module `<m>.log=0`.

## Phase 4. Request input inspection

- [ ] **4.1 [TDD]** SQLi in GET logged in `monitor` mode.
- [ ] **4.2 [TDD]** POST, COOKIE, URI path, query string (URI from `$_SERVER` in RINIT, ADR-001).
- [ ] **4.3 [TDD]** Multipart: text fields only.
- [ ] **4.4 [TDD]** Selected headers (from `$_SERVER` in RINIT, ADR-001).
- [ ] **4.5 [TDD]** Keys and nested arrays, depth limit.
- [ ] **4.6 [TDD]** Length/count limits → `limit_exceeded`.
- [ ] **4.7 [TDD]** `block`: status code, script does not run (ADR-002; tests: FPM + OPcache, worker stays alive per FPM log, Valgrind clean on bailout).
- [ ] **4.7a [TDD]** Block response body: formats, templates, optional header.
- [ ] **4.8 [TDD]** `sanitize` strategies for XSS and SQLi.
- [ ] **4.9 [TDD]** Per-class modes, strictest reaction wins.
- [ ] **4.10 [TDD]** Exclusions (global and per module, glob).
- [ ] **4.11 [TDD]** ext/filter compatibility.
- [ ] **4.12 [TDD]** CLI inspection off by default.
- [ ] **4.13 [TDD]** Switching off at every level has no effect.
- [ ] **4.14 [TDD]** `fail_mode=open|closed`.
- [ ] **4.15 [TDD]** `trusted_proxies` for client IP.

## Phase 5. PHP API

- [ ] **5.1 [TDD]** `InletGuard\check_sqli()`.
- [ ] **5.2 [TDD]** `InletGuard\check_xss()`.
- [ ] **5.3 [TDD]** `InletGuard\inspect()` for strings and arrays.
- [ ] **5.4 [TDD]** `InletGuard\events()`, `request_id()`.
- [ ] **5.5 [TDD]** Constants, `SqlInjectionException`.
- [ ] **5.6 [TDD]** `api_log`; the API never blocks.
- [ ] **5.7 [INFRA]** Stub file and arginfo for 7.x and 8.x.

## Phase 6. SQL function hooks (RASP)

- [ ] **6.1 [TDD]** SQL tokenizer (MySQL, PostgreSQL, SQLite dialects).
- [ ] **6.2 [TDD]** Structural check: input must stay inside one literal.
- [ ] **6.3 [TDD]** Per-request store of user values.
- [ ] **6.4 [TDD]** `SQLite3` hooks.
- [ ] **6.5 [TDD]** Block via exception or `false`.
- [ ] **6.6 [TDD]** `PDO` hooks.
- [ ] **6.7 [TDD]** `mysqli` hooks.
- [ ] **6.8 [TDD]** `pgsql` hooks.
- [ ] **6.9 [TDD]** Missing extensions are skipped.
- [ ] **6.10 [TDD]** Original handlers preserved, Xdebug compatibility.
- [ ] **6.11 [TDD]** `sqli.sink.enabled=0`.

## Phase 7. Hardening, performance, first release (v1.0.0)

- [ ] **7.1 [TDD]** libFuzzer targets.
- [ ] **7.2 [TDD]** Full Valgrind and ASan runs, zero leaks.
- [ ] **7.3 [TDD]** Benchmarks as tests with budgets.
- [ ] **7.4 [TDD]** WAF bypass corpus.
- [ ] **7.5 [INFRA]** Integration stand (nginx + FPM, Apache, LiteSpeed; WordPress; sqlmap).
- [ ] **7.5a [TDD]** End-to-end "works out of the box" test.
- [ ] **7.6 [INFRA]** Documentation (`docs/quickstart.md`, configuration, deployment, log format, API, compatibility).
- [ ] **7.7 [INFRA]** Final packaging (PECL, PIE, example ini files, release notes).
- [ ] **7.8 [INFRA]** First public release v1.0.0. **No releases are published before this task.**

---

# Attack class coverage (after v1.0)

> Catalog: PRD section 10. Per-class checklist: corpus first (red), core
> tokenizer unit tests, phpt for every sink in `monitor` and `block`, fuzz
> targets, module descriptor with profile defaults and build flag,
> performance budget, "disabled → no effect" tests, `docs/attacks/<m>.md`.

## Phase 8. Generalized architecture and more input (v1.1)

- [ ] **8.1 [SPIKE]** Cost of hundreds of hooks → ADR-004.
- [ ] **8.2 [TDD]** User input registry (taint-lite, Aho–Corasick).
- [ ] **8.3 [TDD]** Declarative sink registry and single hook dispatcher.
- [ ] **8.4 [INFRA]** Module template and generator.
- [ ] **8.5 [TDD]** JSON body inspection.
- [ ] **8.6 [TDD]** XML and raw body inspection.
- [ ] **8.7 [TDD]** HPP and input anomalies.
- [ ] **8.8 [TDD]** Scanner and reconnaissance detection.
- [ ] **8.9 [TDD]** Per-class corpus metrics in CI.
- [ ] **8.10 [TDD]** Class flags in `inspect()`.
- [ ] **8.11 [TDD]** `learn` module.
- [ ] **8.12 [TDD]** Path template normalizer.
- [ ] **8.13 [TDD]** Learning fields in events.
- [ ] **8.14 [TDD]** `inletguard-learn` CLI.
- [ ] **8.15 [TDD]** Learning poisoning protection.

## Phase 9. File system: LFI, RFI, uploads, webshells (v1.2)

- [ ] **9.1 [TDD]** Path normalizer and base directory check.
- [ ] **9.2 [TDD]** LFI/RFI input signatures.
- [ ] **9.3 [SPIKE]** Hooking `include/require`, OPcache compatibility → ADR-005.
- [ ] **9.4 [TDD]** `include/require` sink.
- [ ] **9.5 [TDD]** File read function sinks.
- [ ] **9.6 [TDD]** File modification function sinks.
- [ ] **9.7 [TDD]** Stream wrapper policy.
- [ ] **9.8 [TDD]** `$_FILES` inspection.
- [ ] **9.9 [TDD]** `move_uploaded_file` sink.
- [ ] **9.10 [TDD]** `upload.noexec_paths`.
- [ ] **9.11 [TDD]** Webshell write detection.
- [ ] **9.12 [TDD]** Known webshell signatures at compile time.
- [ ] **9.13 [TDD]** Zip Slip.

## Phase 10. Command and code injection (v1.3)

- [ ] **10.1 [TDD]** Shell tokenizer.
- [ ] **10.2 [TDD]** Structural check: input stays one argument.
- [ ] **10.3 [TDD]** `exec`/`system`/`passthru`/`shell_exec`/`popen`/`proc_open`/`pcntl_exec` sinks.
- [ ] **10.4 [TDD]** Command injection input signatures.
- [ ] **10.5 [TDD]** Argument/environment injection (`mail()`, `putenv`, `imap_open`).
- [ ] **10.6 [TDD]** Minimal PHP tokenizer for `eval`.
- [ ] **10.7 [TDD]** `eval` sink via `zend_compile_string`.
- [ ] **10.8 [TDD]** String `assert` (7.x), `create_function` (≤ 7.4).
- [ ] **10.9 [TDD]** Dynamic calls with user-controlled function names.

## Phase 11. Deserialization, XXE, SSRF (v1.4)

- [ ] **11.1 [TDD]** `serialize()` format parser.
- [ ] **11.2 [TDD]** `unserialize` sink, `allowed_classes` policy, gadget classes.
- [ ] **11.3 [TDD]** Serialized object input signatures.
- [ ] **11.4 [TDD]** phar deserialization via file functions.
- [ ] **11.5 [TDD]** DOCTYPE / external entity detector, XML bombs.
- [ ] **11.6 [TDD]** libxml sinks (DOM, SimpleXML, XMLReader, xml_parse).
- [ ] **11.7 [TDD]** URL and IP normalizers, CIDR matcher.
- [ ] **11.8 [TDD]** SSRF policy (private ranges, metadata, schemes, allow/deny hosts).
- [ ] **11.9 [TDD]** curl, streams, sockets, `SoapClient` sinks.
- [ ] **11.10 [SPIKE]** Post-DNS checks, DNS rebinding, redirects → ADR-006.
- [ ] **11.11 [TDD]** ADR-006 implementation.

## Phase 12. Other injections and headers (v1.5)

- [ ] **12.1 [TDD]** LDAP filter tokenizer and sinks.
- [ ] **12.2 [TDD]** XPath tokenizer and sinks.
- [ ] **12.3 [TDD]** NoSQL operator injection, MongoDB sinks.
- [ ] **12.4 [TDD]** SSTI heuristics.
- [ ] **12.5 [TDD]** ReDoS / regex injection.
- [ ] **12.6 [TDD]** CRLF and email header injection.
- [ ] **12.7 [TDD]** Open redirect.
- [ ] **12.8 [TDD]** Variable overwrite (`extract`, `parse_str`).
- [ ] **12.9 [TDD]** Type juggling heuristics.

## Phase 13. Response analysis (v2.0)

- [ ] **13.1 [SPIKE]** Output handler overhead and compatibility → ADR-007.
- [ ] **13.2 [TDD]** HTML context tokenizer.
- [ ] **13.3 [TDD]** Reflected XSS in output.
- [ ] **13.4 [TDD]** Information leakage detection and masking.
- [ ] **13.5 [TDD]** Security response headers.
- [ ] **13.6 [TDD]** Content-type filter, size limit, budget.

## Phase 14. IGR rules (virtual patching) and hardening (v2.1)

- [ ] **14.1 [INFRA]** English spec `docs/rules.md`, `docs/rules.ebnf`, examples.
- [ ] **14.2 [TDD]** Valid/invalid rule corpus with golden error messages.
- [ ] **14.3 [TDD]** Lexer (fuzzed).
- [ ] **14.4 [TDD]** Parser → AST (fuzzed).
- [ ] **14.5 [TDD]** Error recovery and messages.
- [ ] **14.6 [TDD]** Semantic validation.
- [ ] **14.7 [TDD]** Regex abstraction (PCRE1 for 7.0–7.2, PCRE2 for 7.3+).
- [ ] **14.8 [TDD]** Compilation and indexes.
- [ ] **14.9 [TDD]** Field provider interface, collection semantics.
- [ ] **14.10 [TDD]** Operators.
- [ ] **14.11 [TDD]** Transforms.
- [ ] **14.12 [TDD]** Checks (`is tainted`, `is sqli/xss`, types, `breaks`).
- [ ] **14.13 [TDD]** Action resolution (`allow`, `set_mode`, `dry_run`, ceiling).
- [ ] **14.14 [TDD]** Loading directives and errors, `phpinfo()`.
- [ ] **14.15 [TDD]** Zend field provider.
- [ ] **14.16 [TDD]** `request` stage.
- [ ] **14.17 [TDD]** `sink` stage, `BlockedException`.
- [ ] **14.18 [TDD]** ini exclusions compiled into internal `allow` rules.
- [ ] **14.19 [TDD]** `response` stage.
- [ ] **14.20 [TDD]** ZTS read-only rule sharing.
- [ ] **14.21 [TDD]** `rules_lint()`, `rules_eval()`, `rules_loaded()`.
- [ ] **14.22 [INFRA]** `inletguard-rules lint|test` CLI.
- [ ] **14.22a [TDD]** `inletguard-learn --format=igr`.
- [ ] **14.23 [TDD]** Performance with 1000+ rules.
- [ ] **14.24 [TDD]** Dangerous function policy pack.
- [ ] **14.25 [TDD]** CVE rule packs.
- [ ] **14.26 [TDD]** Session hardening.
- [ ] **14.27 [TDD]** Cookie signing/encryption.

## Phase 15. Behavioral protection and counter storage (v2.2 / v2.3)

- [ ] **15.1 [SPIKE]** Shared memory across workers and SAPIs → ADR-008.
- [ ] **15.2 [TDD]** Counter store interface and window algorithm.
- [ ] **15.3 [TDD]** `shm` backend.
- [ ] **15.4 [TDD]** IP allow/deny lists.
- [ ] **15.5 [TDD]** `ratelimit` module.
- [ ] **15.6 [TDD]** `bruteforce` module.
- [ ] **15.7 [TDD]** Auto-ban.
- [ ] **15.8 [TDD]** Parameter profiles and anomaly detection.
- [ ] **15.9 [TDD]** Minimal RESP2 client.
- [ ] **15.10 [TDD]** `redis` backend.
- [ ] **15.11 [TDD]** memcached client and backend.
- [ ] **15.12 [TDD]** `hybrid` mode.
- [ ] **15.13 [TDD]** Backend unavailability handling.
- [ ] **15.14 [TDD]** Secrets handling.
- [ ] **15.15 [TDD]** Performance budgets for remote backends.

---

## Backlog
- Offline rule converters from Snuffleupagus / ModSecurity.
- IGR `version 2`: user function sinks, cross-rule variables, hot reload.
- `.igr` syntax highlighting for editors.
- TLS for Redis/Memcached, Redis Sentinel / Cluster.
- Compression of rotated logs.
- CSRF heuristics.
- syslog / SIEM export.
