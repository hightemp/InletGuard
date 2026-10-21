#!/bin/sh
# SPDX-License-Identifier: MIT
# Download the pinned PHP source tarball for a branch and verify its SHA256.
# Prints the tarball path.
#
#   fetch-php.sh <branch> <download-dir>

set -eu

branch=$1 dir=$2
root=$(cd "$(dirname "$0")/../.." && pwd)
line=$(awk -v b="$branch" '$1 == b { print $2, $3 }' "$root/scripts/release/php-versions.txt")
if [ -z "$line" ]; then
	echo "unknown PHP branch: $branch" >&2
	exit 2
fi
version=${line% *} sha=${line#* }

mkdir -p "$dir"
tarball="$dir/php-$version.tar.xz"
if [ ! -f "$tarball" ]; then
	curl -fsSL --retry 3 -o "$tarball.part" "https://www.php.net/distributions/php-$version.tar.xz"
	mv "$tarball.part" "$tarball"
fi
echo "$sha  $tarball" | sha256sum -c --quiet - >&2
echo "$tarball"
