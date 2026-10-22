#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for scripts/check-tidy.sh: clang-tidy with the repository's
# .clang-tidy over src/core. Set CLANG_TIDY to the pinned binary, e.g.
#   CLANG_TIDY="uvx clang-tidy==18.1.8" sh tests/scripts/test-check-tidy.sh

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CHECK="$ROOT/scripts/check-tidy.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

# make_tree <name>: .clang-tidy, empty src/core and the vendored headers
make_tree()
{
	dir="$TMP/$1"
	mkdir -p "$dir/src/core" "$dir/third_party/libinjection"
	cp "$ROOT/.clang-tidy" "$dir/"
	cp "$ROOT"/third_party/libinjection/*.h "$dir/third_party/libinjection/"
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
	elif [ -n "${4:-}" ] && ! printf '%s' "$out" | grep -q -- "$4"; then
		echo "FAIL $1: output does not mention '$4': $out"
		failures=$((failures + 1))
	else
		echo "PASS $1"
	fi
}

t=$(make_tree real)
cp "$ROOT"/src/core/* "$t/src/core/"
expect "the current src/core is clean" 0 "$t"

t=$(make_tree uninit)
printf 'int f(int c)\n{\n\tint x;\n\tif (c) {\n\t\tx = 1;\n\t}\n\treturn x;\n}\n' >"$t/src/core/uninit.c"
expect "uninitialized value fails and names the file" 1 "$t" "uninit.c"

t=$(make_tree double_free)
printf '#include <stdlib.h>\nvoid f(void)\n{\n\tchar *p = malloc(4);\n\tfree(p);\n\tfree(p);\n}\n' \
	>"$t/src/core/dfree.c"
expect "double free fails" 1 "$t" "dfree.c"

t=$(make_tree signed_char)
printf 'int f(const char *s)\n{\n\tchar c = s[0];\n\tint i = c;\n\treturn i;\n}\n' >"$t/src/core/sc.c"
expect "signed char misuse fails" 1 "$t" "bugprone-signed-char-misuse"

t=$(make_tree header)
printf 'static inline int h(const char *s)\n{\n\tchar c = s[0];\n\tint i = c;\n\treturn i;\n}\n' >"$t/src/core/bad.h"
printf '#include "core/bad.h"\nint f(const char *s)\n{\n\treturn h(s);\n}\n' >"$t/src/core/user.c"
expect "a bug in a src/core header fails" 1 "$t" "bad.h"

t=$(make_tree third_party)
printf 'static inline int tp(const char *s)\n{\n\tchar c = s[0];\n\tint i = c;\n\treturn i;\n}\n' \
	>"$t/third_party/libinjection/tp_bad.h"
printf '#include "tp_bad.h"\nint f(const char *s)\n{\n\treturn tp(s);\n}\n' >"$t/src/core/user.c"
expect "third_party headers are not checked" 0 "$t"

t=$(make_tree missing)
printf 'int ok;\n' >"$t/src/core/ok.c"
out=$(CLANG_TIDY=/nonexistent/clang-tidy sh "$CHECK" "$t" 2>&1)
if [ $? -ne 0 ] && printf '%s' "$out" | grep -q 'clang-tidy'; then
	echo "PASS a missing clang-tidy fails"
else
	echo "FAIL a missing clang-tidy must fail: $out"
	failures=$((failures + 1))
fi

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all check-tidy tests passed"
