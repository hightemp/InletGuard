#!/bin/sh
# SPDX-License-Identifier: MIT
#
# License hygiene checks (NFR-9):
#  - every own source/build file carries an SPDX header in its first 3 lines;
#  - LICENSE and THIRD_PARTY_NOTICES exist;
#  - every library in third_party/ ships a license file, and none is GPL/LGPL.

set -eu

cd "$(dirname "$0")/.."

status=0

for f in LICENSE THIRD_PARTY_NOTICES; do
	if [ ! -f "$f" ]; then
		echo "missing $f"
		status=1
	fi
done

for f in $(git ls-files '*.c' '*.h' '*.m4' '*.sh' 'Makefile*' '**/Makefile*' | grep -v '^third_party/' | sort -u); do
	if ! head -n 3 "$f" | grep -q 'SPDX-License-Identifier:'; then
		echo "missing SPDX header: $f"
		status=1
	fi
done

if [ -d third_party ]; then
	for lib in third_party/*/; do
		[ -d "$lib" ] || continue
		license=$(ls "$lib" | grep -iE '^(LICENSE|COPYING)' | head -n 1 || true)
		if [ -z "$license" ]; then
			echo "missing license file in $lib"
			status=1
			continue
		fi
		if grep -qiE 'GNU (LESSER |LIBRARY )?GENERAL PUBLIC LICENSE' "$lib$license"; then
			echo "GPL/LGPL code is not allowed: $lib"
			status=1
		fi
	done
fi

if [ "$status" -eq 0 ]; then
	echo "license checks passed"
fi
exit "$status"
