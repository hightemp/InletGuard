#!/bin/sh
# SPDX-License-Identifier: MIT
# Print the InletGuard version from src/core/ig_version.h (single source of truth).

set -eu

header="$(cd "$(dirname "$0")/../.." && pwd)/src/core/ig_version.h"

field()
{
	sed -n "s/^#define IG_VERSION_$1 \\(.*\\)\$/\\1/p" "$header"
}

suffix=$(field SUFFIX | tr -d '"')
echo "$(field MAJOR).$(field MINOR).$(field PATCH)$suffix"
