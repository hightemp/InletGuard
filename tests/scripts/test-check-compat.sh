#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for scripts/check-compat.sh: PHP version checks are allowed only in
# src/zend/php_compat.h.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CHECK="$ROOT/scripts/check-compat.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

# make_tree <name>: minimal source tree with a compliant php_compat.h
make_tree()
{
	dir="$TMP/$1"
	mkdir -p "$dir/src/zend" "$dir/src/core"
	printf '#if PHP_VERSION_ID >= 70200\n#endif\n' >"$dir/src/zend/php_compat.h"
	printf 'int ok;\n' >"$dir/inletguard.c"
	echo "$dir"
}

# expect <description> <expected exit: 0|1> <tree> [pattern expected in output]
expect()
{
	out=$(sh "$CHECK" "$3" 2>&1)
	status=$?
	if [ "$2" = 0 ] && [ "$status" -ne 0 ]; then
		echo "FAIL $1: expected success, got $status: $out"
		failures=$((failures + 1))
	elif [ "$2" = 1 ] && [ "$status" -eq 0 ]; then
		echo "FAIL $1: expected failure, got success"
		failures=$((failures + 1))
	elif [ -n "${4:-}" ] && ! printf '%s' "$out" | grep -q "$4"; then
		echo "FAIL $1: output does not mention '$4': $out"
		failures=$((failures + 1))
	else
		echo "PASS $1"
	fi
}

t=$(make_tree clean)
expect "version checks only in php_compat.h pass" 0 "$t"

t=$(make_tree violation)
printf '#if PHP_VERSION_ID >= 80000\n#endif\n' >"$t/src/core/bad.c"
expect "PHP_VERSION_ID elsewhere fails and names the file" 1 "$t" "src/core/bad.c"

t=$(make_tree other_macro)
printf '#if ZEND_MODULE_API_NO > 1\n#endif\n' >"$t/inletguard.c"
expect "ZEND_MODULE_API_NO elsewhere fails" 1 "$t" "inletguard.c"

t=$(make_tree major)
printf '#if PHP_MAJOR_VERSION == 8\n#endif\n' >"$t/src/zend/hooks.h"
expect "PHP_MAJOR_VERSION in another zend header fails" 1 "$t" "src/zend/hooks.h"

t=$(make_tree comment)
printf '/* PHP_VERSION_ID is not used here */\nint x;\n' >"$t/src/core/c.c"
expect "mention in a comment also fails" 1 "$t" "src/core/c.c"

t=$(make_tree third_party)
mkdir -p "$t/third_party/lib"
printf '#if PHP_VERSION_ID > 0\n#endif\n' >"$t/third_party/lib/x.c"
expect "third_party is ignored" 0 "$t"

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all check-compat tests passed"
