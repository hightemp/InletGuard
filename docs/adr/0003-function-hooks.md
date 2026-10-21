# ADR-003: Hooking internal functions and methods

- Status: accepted
- Date: 2026-10-21
- Task: TASKS 0.10 (spike)
- Evidence: throw-away prototype on PHP 7.0.33 and 8.4.26 (CLI — the
  mechanism is SAPI independent): PDO and SQLite3 on in-memory sqlite,
  `mysqli` as a shared extension loaded after the prototype, with and without
  OPcache (8.4: tracing JIT), with Xdebug 2.6.1 (7.0) and 3.5.3 (8.4).

## Context

FR-3 and every later sink (phases 9–12) need to run code before internal
functions and methods execute (`PDO::query`, `mysqli_query`,
`SQLite3::query`, later `system`, `unserialize`, `preg_*`, …), inspect their
arguments, and either call the original or refuse with an exception.

## Findings

| Question | Result (7.0 and 8.4) |
|---|---|
| Replace `zend_internal_function.handler`, call the saved original | works for functions and for methods (class `function_table`); results of normal calls are unchanged |
| Read arguments without parsing them | `ZEND_CALL_ARG(execute_data, n)` and `ZEND_NUM_ARGS()` work on both |
| Refuse a call | throwing (`zend_throw_exception`) and returning without calling the original produces a normal, catchable exception at the caller's line |
| Aliases | `mysqli_query` and `mysqli::query` are separate entries with one C handler — each must be hooked |
| Load order | in our MINIT a later-loaded shared extension's **functions are already registered** (at load time), but its **classes are not** (registered in its own MINIT): `mysqli::query` was missing. Installing in the first RINIT found every target (7 of 7) |
| OPcache / JIT | all 5000 calls in a hot loop were intercepted, also with the 8.4 tracing JIT — the handler is read at call time |
| Xdebug 2.6.1 / 3.5.3 | hooks fire, traces still record the calls, no crashes |
| **PHP 8.4 frameless functions** | the compiler emits `ZEND_FRAMELESS_ICALL` for some functions with a fixed arity, which **bypasses `handler` even without OPcache**: `preg_match('/x/', 'x')` was not intercepted, while the 3-argument form, `call_user_func()` and `$f()` were. Clearing `frameless_function_infos` of the hooked function in MINIT (before any compilation, including OPcache preloading) restores interception, with and without OPcache |

Frameless functions in PHP 8.4: `class_exists`, `dechex`, `dirname`,
`implode`, `in_array`, `is_numeric`, `max`, `min`, `preg_match`,
`preg_replace`, `property_exists`, `str_contains`, `str_replace`,
`str_starts_with`, `strpos`, `strstr`, `strtr`, `substr`, `trim`.

## Decision

1. **Mechanism:** replace `internal_function.handler` of every sink function
   and method with a wrapper that inspects the arguments
   (`ZEND_CALL_ARG`) and then calls the saved original handler, or throws
   (`InletGuard\SqlInjectionException` / `BlockedException`) without calling
   it.
2. **When:** in the **first RINIT**, after every extension (including shared
   ones loaded after us) has completed MINIT — the same point as the compile
   hook of ADR-002. Installation is **idempotent** (a function whose handler is
   already our wrapper is skipped) and tracked per thread: under ZTS each
   thread may own a copy of the function table while internal class entries
   may be shared, so every thread installs in its own first RINIT and
   re-installing on a shared entry must be a no-op.
3. **Frameless functions (PHP ≥ 8.4):** for every hooked function that has
   `frameless_function_infos`, clear it **in MINIT**, before any script is
   compiled. This trades the frameless fast path for interception only for
   functions we actually hook. The check lives in `php_compat.h`.
4. **Lookup of the original handler** must be O(1) — not a name search per
   call as in the prototype. The sink registry (TASKS 8.3) keeps the original
   per `zend_function` (e.g. via a reserved resource slot); to be confirmed
   when 8.3 is implemented.
5. Missing targets (extension not loaded, function absent in this PHP
   version) are skipped silently at startup (FR-9.4).

## Consequences

- Phase 6 (SQL hooks) uses this design directly; tasks 6.4–6.10 test it with
  PDO/SQLite3 always and mysqli/pgsql with services.
- The sink registry (8.3) and every later sink inherit points 2–4; sinks on
  frameless functions (`preg_*` in 12.5, string functions) require point 3.
- ZTS behaviour with real threads (Apache worker/event MPM with ZTS PHP,
  FrankenPHP) was not exercised in the spike; tasks 6.10 and 14.20 must test
  it.
- Compatibility with Xdebug 2.x and 3.x is confirmed for the hooking itself;
  phpt with Xdebug remain in 6.10.
