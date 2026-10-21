#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Publishing policy (PRD 9.2). Publishing requires all of:
#   - a SemVer tag ref: refs/tags/vX.Y.Z[-pre]
#   - major version >= 1 (no releases before v1.0.0)
#   - repository variable RELEASES_ENABLED=true
#
# Prints "decision=publish" or "decision=dry-run" on stdout (append it to
# $GITHUB_OUTPUT) and exits 0; otherwise exits 1. Explanations go to stderr.
#
#   check-policy.sh <git-ref> <releases_enabled>

set -eu

ref=${1:-} enabled=${2:-}

case "$ref" in
refs/tags/*) ;;
*)
	echo "not a tag ($ref): dry run, nothing to publish" >&2
	echo "decision=dry-run"
	exit 0
	;;
esac

tag=${ref#refs/tags/}
if ! printf '%s\n' "$tag" | grep -Eq '^v[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$'; then
	echo "refusing to publish: tag '$tag' is not a SemVer tag vX.Y.Z[-pre]" >&2
	exit 1
fi

major=${tag#v}
major=${major%%.*}
if [ "$major" -lt 1 ]; then
	echo "refusing to publish: '$tag' is before v1.0.0, releases start with v1.0.0" >&2
	exit 1
fi

if [ "$enabled" != "true" ]; then
	echo "refusing to publish: repository variable RELEASES_ENABLED is not 'true'" >&2
	exit 1
fi

echo "publishing $tag" >&2
echo "decision=publish"
