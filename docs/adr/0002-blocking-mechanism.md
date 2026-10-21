# ADR-002: Blocking a request before the script runs

- Status: accepted
- Date: 2026-10-21
- Task: TASKS 0.9 (spike)
- Evidence: throw-away prototype extension, PHP 7.0.33 and 8.4.26/8.4.27,
  under CGI, PHP-FPM (one worker, several requests) and Apache mod_php
  (prefork), each with and without OPcache; CGI also on a ZTS build (8.4).

## Context

The decision to block is known in RINIT (ADR-001: the input filter runs
before RINIT). Blocking must send our response (status `block_status`, our
body) and run **no line** of the user script, on every supported SAPI and
version, with OPcache enabled (the common production setup), without hurting
unblocked requests and without killing worker processes.

## Candidates and results

Blocked request = `?block=1`; "ran" = the script created its marker file.

| Candidate | CGI | FPM, no OPcache | FPM + OPcache | Apache + OPcache | Notes |
|---|---|---|---|---|---|
| 1. RINIT returns `FAILURE` | 403, not ran | **200, empty body** | same | — | no block page; **FPM worker exits** (code 70) |
| 2. RINIT: send response + `zend_bailout()` | 403, not ran | 403, not ran | 403, not ran | — | **FPM worker exits (code 70) after every blocked request** |
| 3. `zend_compile_file` wrapper installed in MINIT | 403, not ran | 403, not ran | **200, script ran** | **200, script ran** | OPcache wraps compile later and serves cached scripts without calling us |
| 4. `zend_compile_file` wrapper installed after OPcache (first RINIT) | 403, not ran | 403, not ran | 403, not ran | 403, not ran | no worker exits, no crashes, next request fine |
| 5. `zend_execute_ex` wrapper | 403, not ran | 403, not ran | 403, not ran | — | works, but overriding `zend_execute_ex` disables the VM's fast userland calls for **every** request |

Results were identical on 7.0 and 8.4. ZTS (8.4, CGI): candidates 4 and 5 block.

## Decision

**Candidate 4.**

1. The `zend_compile_file` wrapper is installed **once per process, lazily, in
   the first RINIT** — after all zend_extensions (OPcache) have installed
   theirs, so ours is the outermost wrapper and runs before OPcache's cache
   lookup. Under ZTS the installation is guarded by a mutex and a flag, so a
   thread always compiles after its own RINIT has seen the hook installed.
2. On the **first compilation of a blocked request** (the main script, or
   `auto_prepend_file` if configured — both before any user code) the wrapper
   sets `SG(sapi_headers).http_response_code`, adds the content type header,
   writes the block body with `php_output_write()` and calls `zend_bailout()`.
   This is the path `exit()` takes on PHP 7.x: shutdown functions, output
   flushing and request shutdown run normally, and the worker keeps serving.
3. Unblocked requests only pay one flag check per compilation.
4. RINIT itself never fails and never bails out.

## Rejected

- Candidates 1 and 2: under FPM a failed request startup terminates the worker
  (`exited with code 70`). Every blocked attack would cost a worker restart —
  the protection would amplify a DoS. Candidate 1 also sends no block page.
- Candidate 3: bypassed by OPcache for cached scripts — a silent hole in the
  most common production configuration.
- Candidate 5: correct, but slows down every userland function call on every
  request. Kept only as a documented fallback should the compile hook ever
  prove insufficient.

## Lessons from the spike

- **Never keep pointers into `environ`** (e.g. the result of `getenv()` in
  MINIT): PHP-FPM rewrites the environment area for the process title in
  worker processes. The first run of this spike was invalid because of exactly
  that; copy such strings.
- **Read logs, not only responses**: candidate 2 looked correct from the
  client side (403, next request served) — only the FPM log showed that the
  worker died and was respawned each time.

## Consequences and follow-ups

- Task 4.7 implements this design; its tests must cover FPM with OPcache
  (blocked request → 403, no execution, worker still alive), and Valgrind must
  confirm no leak of the file handle when bailing out of compilation.
- Interaction with other compile wrappers installed later than our first
  RINIT is limited to ones that cache compiled scripts (OPcache-like); none
  are known. Documented in `docs/compatibility.md`.
- LSAPI could not be set up in the spike; the mechanism is Zend-level and SAPI
  independent, and is verified on the integration stand (TASKS 7.5).
