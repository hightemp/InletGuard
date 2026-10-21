#!/bin/sh
# SPDX-License-Identifier: MIT
# Verify that every vendored library in third_party/ is unmodified: its files
# must match the SHA256 list in README.inletguard.md, with no extra files.
#
#   sh scripts/check-vendor.sh [source-root]

set -eu

root=${1:-$(cd "$(dirname "$0")/.." && pwd)}
status=0

for lib in "$root"/third_party/*/; do
	[ -d "$lib" ] || continue
	name=$(basename "$lib")
	readme="$lib/README.inletguard.md"
	if [ ! -f "$readme" ]; then
		echo "third_party/$name: missing README.inletguard.md"
		status=1
		continue
	fi
	listed=$(grep -E '^[0-9a-f]{64}  ' "$readme" || true)
	if ! out=$(cd "$lib" && printf '%s\n' "$listed" | sha256sum -c --quiet - 2>&1); then
		echo "third_party/$name: checksum mismatch:"
		echo "$out"
		status=1
	fi
	for f in $(cd "$lib" && ls -A); do
		[ "$f" = README.inletguard.md ] && continue
		if ! printf '%s\n' "$listed" | awk '{ print $2 }' | grep -qx -- "$f"; then
			echo "third_party/$name: unlisted file $f"
			status=1
		fi
	done
done

[ "$status" -eq 0 ] && echo "vendor checks passed"
exit "$status"
