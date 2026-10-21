# ADR-001: Intercepting request input

- Status: accepted
- Date: 2026-10-21
- Task: TASKS 0.8 (spike)
- Evidence: throw-away prototype extension run on PHP 7.0.33 and 8.4.26/8.4.27
  under CGI and PHP-FPM, with ext/filter built in (the default in official
  builds).

## Context

FR-1 requires inspecting GET, POST (urlencoded and multipart text fields),
COOKIE, URI and selected headers before the script runs, while ext/filter
(`filter_input()`) keeps working (NFR-4). The candidate mechanism is the SAPI
input filter (`sapi_register_input_filter`), the same hook ext/filter uses.

## Findings

| # | Question | 7.0 | 8.4 |
|---|---|---|---|
| 1 | Chaining: save the previous `sapi_module.input_filter` in MINIT, register ours, call the previous one | `filter_input()`, `filter_has_var()` work | same |
| 2 | Order and mutation | we run first; a value changed in our filter is what both `$_GET` and `filter_input()` return | same |
| 3 | Sources passing through the filter | GET, COOKIE, POST urlencoded, multipart **text** fields; multipart files do not (they appear in `$_FILES`) | same |
| 4 | `var` for arrays | full name with brackets: `a[b][c]` (values already URL-decoded) | same |
| 5 | Server variables (`HTTP_*`, `REQUEST_URI`) | **CGI: never filtered** (only `PHP_SELF`); **FPM: filtered only when `$_SERVER` is materialized** (JIT: at compile time, after RINIT) | same |
| 6 | Timing | `input_filter_init` → filter calls → our RINIT | same |
| 7 | `input_filter_init` | called once per request, before the first filter call (also in a long-lived FPM worker) | same |

Signature difference: the `var` parameter is `char *` on 7.0 and
`const char *` on 8.4. Declaring the callback with the
`SAPI_INPUT_FILTER_FUNC()` macro from `SAPI.h` hides it without any version
check.

## Decision

1. **GET, POST (urlencoded and multipart text fields) and COOKIE** are
   inspected in our SAPI input filter, registered in MINIT and declared with
   `SAPI_INPUT_FILTER_FUNC()`. The previously registered filter (ext/filter)
   is saved and always called after ours, with our (possibly sanitized) value.
2. **Per-request state** is reset in our `input_filter_init` callback (it runs
   once per request before any filter call; RINIT is too late). The previous
   `input_filter_init` is chained too.
3. **Sanitizing** happens inside the filter: the value we write is what the
   application and ext/filter see. The decision to *block* is not taken in the
   filter; it is taken in RINIT from the collected events (mechanism: ADR-002).
4. **Headers and URI do not use the input filter.** In RINIT we force
   materialization of `$_SERVER` (`zend_is_auto_global_str("_SERVER")`) and
   read the configured headers and `REQUEST_URI` from it. This works the same
   on every SAPI and happens before the script runs. When `$_SERVER` is
   materialized this way under FPM, the filter is also called for server
   variables; our filter ignores `PARSE_SERVER` and `PARSE_ENV` so nothing is
   inspected twice. The cost of building `$_SERVER` early is paid only when
   header or URI inspection is enabled.
5. **Not covered by the filter** and handled elsewhere: uploaded files
   (`$_FILES`, RINIT, phase 9), JSON/XML/raw bodies (`php://input`, RINIT,
   phase 8).
6. Path-based exclusions can be evaluated inside the filter:
   `SG(request_info).request_uri` is already set when the filter runs.

## Consequences

- No conflict with ext/filter, including `filter_input()`, on all supported
  versions.
- Tasks 4.1–4.5 and 4.8 build on the filter; 4.4 (headers) and the URI part of
  4.2 use the `$_SERVER` path in RINIT; 4.11 (ext/filter compatibility) gets a
  phpt that runs `filter_input()` with InletGuard loaded.
- If another extension registers an input filter after us (loaded later), it
  becomes the head of the chain and must call ours; this is out of our control
  and should be documented (`docs/compatibility.md`).
