#!/bin/sh
# SPDX-License-Identifier: MIT
# Check that a release tag is consistent with the sources (TASKS 0.5f):
# src/core/ig_version.h, CHANGELOG.md and, if present, package.xml.
# Reports every problem and exits 1 if there are any.
#
#   check-version.sh <tag>      e.g. v1.2.0

set -eu

tag=$1
root=$(cd "$(dirname "$0")/../.." && pwd)
release=$(dirname "$0")
want=${tag#v}
problems=""

problem()
{
	problems="$problems
  - $1"
}

have=$(sh "$release/version.sh")
if [ "$have" != "$want" ]; then
	problem "src/core/ig_version.h says $have, tag says $want"
fi
case "$have$want" in
*-dev*) problem "a development version (-dev) cannot be released" ;;
esac

if ! grep -Eq "^## \[$(printf '%s' "$want" | sed 's/[.]/\\./g')\] - [0-9]{4}-[0-9]{2}-[0-9]{2}\$" "$root/CHANGELOG.md" 2>/dev/null; then
	problem "CHANGELOG.md has no '## [$want] - YYYY-MM-DD' section"
elif [ -z "$(sh "$release/changelog-notes.sh" "$want")" ]; then
	problem "CHANGELOG.md section for $want is empty"
fi

if [ -f "$root/package.xml" ]; then
	pkg=$(sed -n 's:.*<release>\([^<]*\)</release>.*:\1:p' "$root/package.xml" | head -n 1)
	if [ "$pkg" != "$want" ]; then
		problem "package.xml release is '$pkg', tag says $want"
	fi
fi

if [ -n "$problems" ]; then
	echo "version check failed for $tag:$problems"
	exit 1
fi
echo "version check passed for $tag"
