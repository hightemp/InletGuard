# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Extension skeleton with `InletGuard\version()`.
- CI on PHP 7.0–8.5 (NTS and ZTS), sanitizers, Valgrind, aarch64.
- Release pipeline (dry run until v1.0.0) with portable builds.
- Bundled libinjection 4.0.0 (BSD-3-Clause), built unmodified.
- `./configure --with-inletguard-modules=LIST` selects the attack modules to
  build; `phpinfo()` shows the compiled-in modules.
