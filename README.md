# InletGuard

InletGuard is a native PHP extension (like `curl` or `gd`) that protects PHP
applications from injection attacks at the runtime level, without changes to
application code.

> **Status: early development.** There are no releases yet. The first public
> release will be **v1.0.0** (SQL injection and XSS protection). Until then the
> release pipeline only builds and verifies artifacts; nothing is published.

## Goals

- Detect SQL injection and XSS in request input before the script runs.
- Hook database functions (`mysqli`, `PDO`, `pgsql`, `sqlite3`) and block
  queries whose structure was changed by user input.
- Work out of the box: `extension=inletguard.so` is the only required setting.
- Configurable reactions per attack class: `monitor`, `block`, `sanitize`.
- JSON Lines event log.
- Later releases add more attack classes (LFI/RFI, command and code injection,
  deserialization, XXE, SSRF, ...), a rule language for virtual patching and
  behavioral protection. See the roadmap in `PRD.md` (Russian).

## Supported platforms

- PHP 7.0, 7.1, 7.2, 7.3, 7.4, 8.0, 8.1, 8.2, 8.3, 8.4
- NTS and ZTS builds
- Linux x86_64 and aarch64
- SAPIs: FPM, CGI, Apache mod_php, LiteSpeed (LSAPI), CLI

## Building from source

Requirements: PHP development files (`phpize`, `php-config`), a C compiler,
`autoconf`, `make`.

```sh
phpize
./configure --enable-inletguard
make
make test
```

Then add to `php.ini` (or a `conf.d` file):

```ini
extension=inletguard.so
```

Check that it is loaded:

```sh
php -r 'var_dump(InletGuard\version());'
```

## Running tests

```sh
make test          # phpt integration tests
make unit          # core C unit tests (no PHP needed)
make unit-asan     # core unit tests with AddressSanitizer/UBSan
```

Run the phpt suite against any PHP version with Docker:

```sh
docker run --rm -v "$PWD:/src:ro" php:7.0-cli sh /src/scripts/ci/phpt.sh
docker run --rm -v "$PWD:/src:ro" php:8.4-zts sh /src/scripts/ci/phpt.sh
docker run --rm -v "$PWD:/src:ro" php:8.4-cli sh /src/scripts/ci/phpt.sh --asan
```

## Documentation

- `PRD.md` — product requirements and design notes (Russian).
- `TASKS.md` — phased implementation plan (Russian).
- `CONTRIBUTING.md` — development process (test-driven development).

## License

MIT, see `LICENSE`. Third-party components are listed in
`THIRD_PARTY_NOTICES`.
