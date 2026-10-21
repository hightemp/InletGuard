#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for the release helper scripts in scripts/release/.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
REL="$ROOT/scripts/release"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

pass()
{
	echo "PASS $1"
}

fail()
{
	echo "FAIL $1"
	failures=$((failures + 1))
}

# (a) version.sh prints the version from src/core/ig_version.h
version=$(sh "$REL/version.sh" 2>&1)
major=$(sed -n 's/^#define IG_VERSION_MAJOR \([0-9]*\)$/\1/p' "$ROOT/src/core/ig_version.h")
if printf '%s' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$' &&
	[ "${version%%.*}" = "$major" ]; then
	pass "version.sh prints the SemVer version from ig_version.h ($version)"
else
	fail "version.sh: '$version'"
fi

# (b) php-versions.txt pins exactly the supported branches
versions="$REL/php-versions.txt"
branches=$(grep -v '^#' "$versions" 2>/dev/null | awk 'NF { print $1 }' | tr '\n' ' ')
if [ "$branches" = "7.0 7.1 7.2 7.3 7.4 8.0 8.1 8.2 8.3 8.4 " ]; then
	pass "php-versions.txt lists branches 7.0-8.4"
else
	fail "php-versions.txt branches: '$branches'"
fi
bad=$(grep -v '^#' "$versions" 2>/dev/null | awk 'NF {
	if (index($2, $1 ".") != 1 || length($3) != 64 || $3 !~ /^[0-9a-f]+$/) print $0
}')
if [ -f "$versions" ] && [ -z "$bad" ]; then
	pass "every pinned version matches its branch and has a SHA256"
else
	fail "malformed php-versions.txt lines: '$bad'"
fi

# (c) package.sh builds an archive with the exact file set
printf 'not a real extension' >"$TMP/inletguard.so"
archive=$(sh "$REL/package.sh" "$TMP/inletguard.so" 8.3 zts aarch64 "$TMP/out" 2>&1)
expected="$TMP/out/inletguard-$version-php8.3-zts-aarch64-linux-gnu.tar.gz"
if [ "$archive" = "$expected" ] && [ -f "$expected" ]; then
	pass "package.sh names the archive after version, PHP branch, variant and arch"
else
	fail "package.sh output: '$archive', expected '$expected'"
fi
base="inletguard-$version-php8.3-zts-aarch64-linux-gnu"
files=$(tar -tzf "$expected" 2>/dev/null | grep -v '/$' | sort | tr '\n' ' ')
want="$base/20-inletguard.ini $base/LICENSE $base/README.md $base/THIRD_PARTY_NOTICES $base/inletguard.so "
if [ "$files" = "$want" ]; then
	pass "archive contains exactly .so, ini, README, LICENSE, THIRD_PARTY_NOTICES"
else
	fail "archive contents: '$files'"
fi

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all release script tests passed"
