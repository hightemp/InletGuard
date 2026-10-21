#!/bin/sh
# SPDX-License-Identifier: MIT
# Vendor libinjection (unmodified) into third_party/libinjection and record
# its origin and file checksums in README.inletguard.md.
#
#   scripts/vendor-libinjection.sh <tag>        e.g. v4.0.0

set -eu

tag=$1
upstream=https://github.com/libinjection/libinjection.git
root=$(cd "$(dirname "$0")/.." && pwd)
dest="$root/third_party/libinjection"
files="libinjection.h libinjection_error.h libinjection_sqli.c libinjection_sqli.h
libinjection_sqli_data.h libinjection_xss.c libinjection_xss.h libinjection_html5.c
libinjection_html5.h"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
git -c advice.detachedHead=false clone -q --depth 1 --branch "$tag" "$upstream" "$work/src"
commit=$(git -C "$work/src" rev-parse HEAD)
date=$(git -C "$work/src" log -1 --format=%cs)

rm -rf "$dest"
mkdir -p "$dest"
for f in $files; do
	cp "$work/src/src/$f" "$dest/"
done
cp "$work/src/COPYING" "$dest/"

# shellcheck disable=SC2086
sums=$(cd "$dest" && sha256sum $files COPYING)
cat >"$dest/README.inletguard.md" <<README
# libinjection (vendored)

- Upstream: $upstream
- Tag: $tag
- Commit: $commit ($date)
- License: BSD-3-Clause, see \`COPYING\`
- Files are copied **unmodified** by \`scripts/vendor-libinjection.sh $tag\`.
  Do not edit them here; \`scripts/check-vendor.sh\` verifies the checksums.

Files (SHA256):

\`\`\`
$sums
\`\`\`
README
echo "vendored libinjection $tag ($commit)"
