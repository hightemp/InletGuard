#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for scripts/release/check-version.sh and changelog-notes.sh, using
# temporary trees with their own ig_version.h, CHANGELOG.md and package.xml.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

# make_tree <name> <major> <minor> <patch> <suffix>: tree with a CHANGELOG
# section for 1.2.0 and copies of the release scripts
make_tree()
{
	dir="$TMP/$1"
	mkdir -p "$dir/src/core" "$dir/scripts/release"
	cp "$ROOT"/scripts/release/*.sh "$dir/scripts/release/"
	cat >"$dir/src/core/ig_version.h" <<EOF
#define IG_VERSION_MAJOR $2
#define IG_VERSION_MINOR $3
#define IG_VERSION_PATCH $4
#define IG_VERSION_SUFFIX "$5"
EOF
	cat >"$dir/CHANGELOG.md" <<'EOF'
# Changelog

## [Unreleased]

## [1.2.0] - 2027-03-01

### Added
- Something useful.

## [1.1.0] - 2027-01-15

### Fixed
- An older fix.
EOF
	echo "$dir"
}

# expect <description> <tree> <tag> <exit> [pattern]
expect()
{
	out=$(sh "$2/scripts/release/check-version.sh" "$3" 2>&1)
	status=$?
	if [ "$status" -ne "$4" ]; then
		echo "FAIL $1: exit $status, output: $out"
		failures=$((failures + 1))
	elif [ -n "${5:-}" ] && ! printf '%s' "$out" | grep -q -- "$5"; then
		echo "FAIL $1: output does not mention '$5': $out"
		failures=$((failures + 1))
	else
		echo "PASS $1"
	fi
}

t=$(make_tree ok 1 2 0 "")
expect "consistent tag, header and changelog" "$t" v1.2.0 0

t=$(make_tree mismatch 1 3 0 "")
expect "tag differs from ig_version.h" "$t" v1.2.0 1 "ig_version.h"

t=$(make_tree dev 1 2 0 "-dev")
expect "development version cannot be released" "$t" v1.2.0-dev 1 "development"

t=$(make_tree nochangelog 1 4 0 "")
expect "missing CHANGELOG section" "$t" v1.4.0 1 "CHANGELOG"

t=$(make_tree empty 1 5 0 "")
printf '# Changelog\n\n## [1.5.0] - 2027-05-01\n\n## [1.4.0] - 2027-04-01\n\n- x\n' >"$t/CHANGELOG.md"
expect "empty CHANGELOG section" "$t" v1.5.0 1 "CHANGELOG"

t=$(make_tree pkg_ok 1 2 0 "")
printf '<package><version><release>1.2.0</release></version></package>\n' >"$t/package.xml"
expect "package.xml matches" "$t" v1.2.0 0

t=$(make_tree pkg_bad 1 2 0 "")
printf '<package><version><release>1.1.0</release></version></package>\n' >"$t/package.xml"
expect "package.xml differs" "$t" v1.2.0 1 "package.xml"

t=$(make_tree multi 1 3 0 "-dev")
expect "all problems are reported" "$t" v1.2.0 1 "development"
expect "all problems are reported (header)" "$t" v1.2.0 1 "ig_version.h"

t=$(make_tree notes 1 2 0 "")
notes=$(sh "$t/scripts/release/changelog-notes.sh" 1.2.0 2>&1)
want=$(printf '### Added\n- Something useful.')
if [ "$notes" = "$want" ]; then
	echo "PASS changelog-notes.sh prints exactly the section body"
else
	echo "FAIL changelog-notes.sh: '$notes'"
	failures=$((failures + 1))
fi

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all version check tests passed"
