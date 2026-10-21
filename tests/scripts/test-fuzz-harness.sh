#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for the fuzzing harness (tests/fuzz/Makefile) using temporary targets,
# so the harness is verified before real fuzz targets exist.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

# run_harness <target-dir>: runs the harness on targets from <target-dir>
run_harness()
{
	make -s -C "$ROOT/tests/fuzz" run FUZZ_SRC_DIR="$1" FUZZ_WORK_DIR="$1/work" FUZZ_SECONDS=5 2>&1
}

pass()
{
	echo "PASS $1"
}

fail()
{
	echo "FAIL $1"
	failures=$((failures + 1))
}

# (a) no targets
mkdir -p "$TMP/none"
out=$(run_harness "$TMP/none")
if [ $? -eq 0 ] && printf '%s' "$out" | grep -q "no fuzz targets yet"; then
	pass "no targets: succeeds and says so"
else
	fail "no targets: $out"
fi

# (b) a safe target
mkdir -p "$TMP/safe"
cat >"$TMP/safe/fuzz_safe.c" <<'EOF'
#include <stddef.h>
#include <stdint.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	(void)data;
	(void)size;
	return 0;
}
EOF
if out=$(run_harness "$TMP/safe"); then
	pass "safe target: succeeds"
else
	fail "safe target: $out"
fi

# (c) a buggy target; the crashing input is in its seed corpus
mkdir -p "$TMP/buggy/corpus/fuzz_buggy"
cat >"$TMP/buggy/fuzz_buggy.c" <<'EOF'
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 3 && memcmp(data, "BUG", 3) == 0) {
		char *p = malloc(size);
		memcpy(p, data, size);
		p[size] = 0; /* heap-buffer-overflow */
		free(p);
	}
	return 0;
}
EOF
printf 'BUG' >"$TMP/buggy/corpus/fuzz_buggy/seed"
if out=$(run_harness "$TMP/buggy"); then
	fail "buggy target: expected failure, harness succeeded"
elif ls "$TMP/buggy/work/artifacts/fuzz_buggy"/crash-* >/dev/null 2>&1; then
	pass "buggy target: fails and keeps the crash input"
else
	fail "buggy target: failed but no crash-* artifact: $out"
fi

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all fuzz harness tests passed"
