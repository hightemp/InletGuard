# ADR-009: Configuration per FPM pool and per virtual host

- Status: accepted
- Date: 2026-10-21
- Task: TASKS 0.13 (spike)
- Evidence: throw-away prototype with a `PHP_INI_SYSTEM` directive whose
  `OnUpdate` handler logs the stage; PHP-FPM 7.0–8.4 with three pools
  (`php_admin_value`, `php_value`, `.user.ini`) and Apache mod_php 7.0 / 8.4
  (prefork, one process) with three virtual hosts (`php_admin_value`,
  `php_value`, `.htaccess`) requested alternately.

## Findings

| Source | FPM | Apache mod_php |
|---|---|---|
| global `php.ini` | visible in MINIT (master) and in every worker | visible in MINIT and every request |
| `php_admin_value` (pool / `<VirtualHost>`) | applied once when the worker starts (stage ACTIVATE); it is the worker's base value (`modified=0`); **not visible in MINIT** | applied per request before RINIT (stage ACTIVATE, `modified=1`, `orig_value` = global value), restored after the request (stage DEACTIVATE); one process serves different hosts correctly |
| `php_value` (pool / `<VirtualHost>`) | applied like `php_admin_value`, **but on PHP 7.0 and 7.1 it makes the `PHP_INI_SYSTEM` directive changeable with `ini_set()` from the script** (fixed in 7.2) | ignored for `PHP_INI_SYSTEM` |
| `.user.ini` / `.htaccess` `php_value` | ignored | ignored |
| `ini_set()` from the script | refused (except the 7.0–7.1 `php_value` case) | refused |

Additional observation (Apache, PHP 8.4 with OPcache): a script compiled
while serving host A returned host A's value from `ini_get()` while serving
host B, although the C side read the correct value. OPcache evaluates
`ini_get()` of `PHP_INI_SYSTEM` directives at compile time, so per-vhost
values are not reliably visible to userland `ini_get()`.

A guard in the `OnUpdate` handler that returns `FAILURE` for
`ZEND_INI_STAGE_RUNTIME` and `ZEND_INI_STAGE_HTACCESS` closes the 7.0–7.1
hole while pool and vhost values (stage ACTIVATE) keep working — verified on
7.0, 7.1 and 8.4.

## Decision

1. **Every `inletguard.*` directive** is `PHP_INI_SYSTEM` **and** its
   `OnUpdate` handler refuses changes at the RUNTIME and HTACCESS stages. The
   application can never weaken protection, on any PHP version, regardless of
   whether the administrator used `php_value` or `php_admin_value`.
2. **Two kinds of directives** (FR-10.5):
   - **Startup** — consumed in MINIT or once per process (input filter,
     frameless handling, module list, rule files, shared memory, counter
     backend): taken **only from the global `php.ini`/`conf.d`**. InletGuard
     remembers the value seen in MINIT; if a request later sees a different
     value (a pool or vhost override), it emits one `config_warning` per
     process ("startup directive, per-pool/vhost value ignored") and keeps the
     MINIT value. Hooks installed in the first RINIT (ADR-002/003) use the
     remembered startup values, never the first served host's values.
   - **Runtime** — read per request in C (`INI_*` macros in RINIT or later):
     may be set per FPM pool and per Apache virtual host with
     `php_admin_value` (or `php_value` in a pool).
3. `InletGuard\config()` and `phpinfo()` report values read in C at request
   time, never through userland `ini_get()`; the documentation warns that
   `ini_get('inletguard.*')` may be stale under OPcache with per-vhost values.
4. Directive classification is part of each module descriptor (TASKS 2.8/2.9)
   and is listed in `docs/configuration.md`.

## Consequences

- TASKS 2.1/2.2: phpt for the stage guard (`ini_set()` refused); an FPM
  integration test with a pool using `php_value` on 7.0/7.1 must show the
  directive cannot be changed from the script.
- TASKS 2.12: precedence tests for pool/vhost values and the
  startup-directive warning.
- LSAPI per-vhost configuration was not tested; it uses the same Zend ini
  stages and is covered on the integration stand (TASKS 7.5).
