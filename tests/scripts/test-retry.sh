#!/bin/sh
# SPDX-License-Identifier: MIT
# Tests for scripts/release/retry.sh.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
RETRY="$ROOT/scripts/release/retry.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
failures=0

# flaky <counter-file> <failures-before-success> <exit-code>
cat >"$TMP/flaky" <<'FLAKY'
#!/bin/sh
n=$(cat "$1" 2>/dev/null || echo 0)
n=$((n + 1))
echo "$n" >"$1"
[ "$n" -gt "$2" ] && exit 0
exit "$3"
FLAKY
chmod +x "$TMP/flaky"

check()
{
	if [ "$2" = "$3" ]; then
		echo "PASS $1"
	else
		echo "FAIL $1: expected '$3', got '$2'"
		failures=$((failures + 1))
	fi
}

sh "$RETRY" 3 0 "$TMP/flaky" "$TMP/c1" 2 7 >/dev/null 2>&1
check "succeeds on the third attempt" "$?" 0
check "ran exactly 3 times" "$(cat "$TMP/c1")" 3

sh "$RETRY" 4 0 "$TMP/flaky" "$TMP/c2" 99 7 >/dev/null 2>&1
check "always failing command fails with its exit code" "$?" 7
check "always failing command ran exactly 4 times" "$(cat "$TMP/c2")" 4

sh "$RETRY" 3 0 "$TMP/flaky" "$TMP/c3" 0 7 >/dev/null 2>&1
check "a succeeding command runs once" "$(cat "$TMP/c3")" 1

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all retry tests passed"
