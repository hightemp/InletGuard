#!/bin/sh
# SPDX-License-Identifier: MIT
# Smoke-test archives against the official php:<branch>-cli and -zts images.
# Runs on the host (needs docker); the architecture is the host's.
#
#   smoke-official.sh <archive-dir> <branch>...

set -eu

dir=$(cd "$1" && pwd)
shift
src=$(cd "$(dirname "$0")/../.." && pwd)

status=0
for b in "$@"; do
	for variant in cli zts; do
		docker run --rm -v "$src:/src:ro" -v "$dir:/dist:ro" "php:$b-$variant" \
			sh /src/scripts/release/smoke.sh php /dist || status=1
	done
done
exit "$status"
