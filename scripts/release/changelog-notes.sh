#!/bin/sh
# SPDX-License-Identifier: MIT
# Print the body of the CHANGELOG.md section for a version (release notes),
# without surrounding blank lines.
#
#   changelog-notes.sh <version>

set -eu

version=$1
root=$(cd "$(dirname "$0")/../.." && pwd)

awk -v v="$version" '
	index($0, "## [" v "]") == 1 { found = 1; next }
	found && /^## / { exit }
	found { lines[n++] = $0 }
	END {
		first = 0; last = n - 1
		while (first <= last && lines[first] ~ /^[[:space:]]*$/) first++
		while (last >= first && lines[last] ~ /^[[:space:]]*$/) last--
		for (i = first; i <= last; i++) print lines[i]
	}
' "$root/CHANGELOG.md"
