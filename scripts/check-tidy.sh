#!/bin/sh
# SPDX-License-Identifier: MIT
#
# clang-tidy over the core (src/core) with the repository's .clang-tidy and
# the core build flags. Vendored headers are not checked (HeaderFilterRegex).
#
#   CLANG_TIDY="pipx run clang-tidy==18.1.8" sh scripts/check-tidy.sh [source-root]

set -eu

root=${1:-$(cd "$(dirname "$0")/.." && pwd)}
tidy=${CLANG_TIDY:-clang-tidy}

# shellcheck disable=SC2086 # CLANG_TIDY may be a command with arguments
if ! $tidy --version >/dev/null 2>&1; then
	echo "check-tidy: '$tidy' not found; install clang-tidy 18.1.8 or set CLANG_TIDY"
	exit 1
fi

cd "$root"
set -- src/core/*.c
if [ ! -e "$1" ]; then
	echo "check-tidy: no sources in src/core"
	exit 1
fi

# shellcheck disable=SC2086
if ! $tidy --quiet --config-file=.clang-tidy "$@" -- \
	-std=c99 -D_POSIX_C_SOURCE=200809L -Isrc -Ithird_party/libinjection; then
	echo "check-tidy: clang-tidy reported problems"
	exit 1
fi
echo "clang-tidy checks passed ($# files)"
