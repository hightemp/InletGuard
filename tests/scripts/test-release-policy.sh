#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for scripts/release/check-policy.sh (PRD 9.2 publishing policy).
# Contract: prints "decision=publish" or "decision=dry-run" and exits 0, or
# prints the reason and exits 1 when a tag must not be published.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
POLICY="$ROOT/scripts/release/check-policy.sh"
failures=0

# expect <ref> <releases_enabled> <exit> <pattern in output>
expect()
{
	out=$(sh "$POLICY" "$1" "$2" 2>&1)
	status=$?
	if [ "$status" -eq "$3" ] && printf '%s' "$out" | grep -q -- "$4"; then
		echo "PASS $1 (enabled=$2) -> $4"
	else
		echo "FAIL $1 (enabled=$2): exit $status, output: $out"
		failures=$((failures + 1))
	fi
}

expect refs/tags/v1.0.0 true 0 "decision=publish"
expect refs/tags/v1.2.3-rc.1 true 0 "decision=publish"
expect refs/tags/v2.10.0 true 0 "decision=publish"
expect refs/tags/v0.9.0 true 1 "before v1.0.0"
expect refs/tags/v0.0.0-test true 1 "before v1.0.0"
expect refs/tags/v1.0.0 false 1 "RELEASES_ENABLED"
expect refs/tags/v1.0.0 "" 1 "RELEASES_ENABLED"
expect refs/tags/v1.0 true 1 "not a SemVer"
expect refs/tags/1.0.0 true 1 "not a SemVer"
expect refs/tags/vX true 1 "not a SemVer"
expect refs/heads/main true 0 "decision=dry-run"
expect refs/pull/5/merge false 0 "decision=dry-run"

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all release policy tests passed"
