#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for scripts/check-vendor.sh: vendored files must match the SHA256
# list in third_party/<lib>/README.inletguard.md.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CHECK="$ROOT/scripts/check-vendor.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

# make_tree <name>: a vendored library with two files and a correct README
make_tree()
{
	lib="$TMP/$1/third_party/lib"
	mkdir -p "$lib"
	printf 'int a;\n' >"$lib/a.c"
	printf 'int b;\n' >"$lib/b.h"
	printf 'license\n' >"$lib/COPYING"
	{
		printf '# lib\n\nFiles (SHA256):\n\n```\n'
		(cd "$lib" && sha256sum a.c b.h COPYING)
		printf '```\n'
	} >"$lib/README.inletguard.md"
	echo "$TMP/$1"
}

# expect <description> <tree> <exit> [pattern]
expect()
{
	out=$(sh "$CHECK" "$2" 2>&1)
	status=$?
	if [ "$status" -ne "$3" ]; then
		echo "FAIL $1: exit $status, output: $out"
		failures=$((failures + 1))
	elif [ -n "${4:-}" ] && ! printf '%s' "$out" | grep -q -- "$4"; then
		echo "FAIL $1: output does not mention '$4': $out"
		failures=$((failures + 1))
	else
		echo "PASS $1"
	fi
}

t=$(make_tree ok)
expect "untouched vendored files pass" "$t" 0

t=$(make_tree modified)
printf 'int a = 1;\n' >"$t/third_party/lib/a.c"
expect "a modified file fails and is named" "$t" 1 "a.c"

t=$(make_tree extra)
printf 'x\n' >"$t/third_party/lib/extra.c"
expect "an unlisted file fails" "$t" 1 "extra.c"

t=$(make_tree missing)
rm "$t/third_party/lib/b.h"
expect "a listed but missing file fails" "$t" 1 "b.h"

t=$(make_tree noreadme)
rm "$t/third_party/lib/README.inletguard.md"
expect "a library without README.inletguard.md fails" "$t" 1 "README.inletguard.md"

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all check-vendor tests passed"
