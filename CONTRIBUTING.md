# Contributing to InletGuard

## Languages

- Code, identifiers, comments, log and error messages, tests, commit
  messages, `README.md`, `docs/` and ADRs are written in **English**.
- `PRD.md` and `TASKS.md` are written in Russian.

## Workflow

1. Pick a task from `TASKS.md`.
2. **Think first.** Write an implementation note in `PRD.md`, section 13:
   the chosen approach, rejected alternatives and the test plan. Do not start
   coding before the note exists.
3. Follow test-driven development for every `[TDD]` task:
   1. **Red** — write the test (C unit test in `tests/unit/` or a `.phpt` in
      `tests/`) and see it fail for the expected reason.
   2. **Green** — write the minimal code that makes it pass.
   3. **Refactor** — clean up with the tests green, run under ASan.
4. `[SPIKE]` tasks are throw-away prototypes; record the outcome as an ADR in
   `docs/adr/`.

## Definition of done

- Tests were written before the code (red → green is visible in history).
- Green on the full matrix: PHP 7.0–8.4 × NTS/ZTS.
- AddressSanitizer/UBSan clean, no compiler warnings with
  `--enable-inletguard-dev` (`-Werror`).
- New files carry an `SPDX-License-Identifier: MIT` header.
- Only permissive third-party code (MIT, BSD, Apache-2.0, zlib); no GPL/LGPL.

## Layout

| Path | Content |
|---|---|
| `inletguard.c`, `php_inletguard.h`, `config.m4` | Extension entry point and build |
| `src/core/` | Pure C99 core, no Zend dependencies; unit-tested |
| `src/zend/` | Zend integration; `php_compat.h` is the only place for `PHP_VERSION_ID` checks |
| `tests/*.phpt` | Integration tests (`make test`) |
| `tests/unit/` | Core unit tests (`make unit`) |
| `scripts/` | CI and maintenance scripts |

## Style

Formatting is enforced with `clang-format` 18.1.8 (`.clang-format`):

```sh
pipx run clang-format==18.1.8 -i $(git ls-files '*.c' '*.h')
```
