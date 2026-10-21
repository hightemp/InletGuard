#!/bin/sh
# SPDX-License-Identifier: MIT
# Package a built extension into a release archive and print its path.
#
#   package.sh <inletguard.so> <php-branch> <nts|zts> <arch> <out-dir>

set -eu

if [ $# -ne 5 ]; then
	echo "usage: $0 <inletguard.so> <php-branch> <nts|zts> <arch> <out-dir>" >&2
	exit 2
fi

so=$1 branch=$2 variant=$3 arch=$4 out=$5
root=$(cd "$(dirname "$0")/../.." && pwd)
version=$(sh "$root/scripts/release/version.sh")
base="inletguard-$version-php$branch-$variant-$arch-linux-gnu"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/$base" "$out"
cp "$so" "$stage/$base/inletguard.so"
cp "$root/packaging/20-inletguard.ini" "$root/README.md" "$root/LICENSE" \
	"$root/THIRD_PARTY_NOTICES" "$stage/$base/"

archive="$(cd "$out" && pwd)/$base.tar.gz"
tar -C "$stage" -czf "$archive" "$base"
echo "$archive"
