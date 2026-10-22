#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for scripts/check-php-matrix.sh: every list of PHP branches in CI,
# release and docs must match scripts/release/php-versions.txt. Each case
# copies the real files into a temporary tree and introduces one drift.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CHECK="$ROOT/scripts/check-php-matrix.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

FILES="scripts/release/php-versions.txt .github/workflows/ci.yml .github/workflows/release.yml
README.md CONTRIBUTING.md .github/pull_request_template.md src/zend/php_compat.h"

branches=$(grep -v '^#' "$ROOT/scripts/release/php-versions.txt" | awk 'NF { print $1 }')
oldest=$(echo "$branches" | head -n 1)
second=$(echo "$branches" | sed -n 2p)
newest=$(echo "$branches" | tail -n 1)
prev=$(echo "$branches" | tail -n 2 | head -n 1)
count=$(echo "$branches" | wc -l)

# make_tree <name>: copy of the files the check reads
make_tree()
{
	dir="$TMP/$1"
	for f in $FILES; do
		mkdir -p "$dir/$(dirname "$f")"
		cp "$ROOT/$f" "$dir/$f"
	done
	echo "$dir"
}

# edit <tree> <file> <sed script>
edit()
{
	sed "$3" "$1/$2" >"$1/$2.new" && mv "$1/$2.new" "$1/$2"
}

# expect <description> <expected exit: 0|1> <tree> [pattern expected in output]
expect()
{
	out=$(sh "$CHECK" "$3" 2>&1)
	status=$?
	if [ "$2" = 0 ] && [ "$status" -ne 0 ]; then
		echo "FAIL $1: expected success, got $status: $out"
		failures=$((failures + 1))
	elif [ "$2" = 1 ] && [ "$status" -eq 0 ]; then
		echo "FAIL $1: expected failure, got success"
		failures=$((failures + 1))
	elif [ -n "${4:-}" ] && ! printf '%s' "$out" | grep -q -- "$4"; then
		echo "FAIL $1: output does not mention '$4': $out"
		failures=$((failures + 1))
	else
		echo "PASS $1"
	fi
}

t=$(make_tree clean)
expect "the repository is consistent ($oldest-$newest)" 0 "$t"

t=$(make_tree phpt)
edit "$t" .github/workflows/ci.yml "/'$second'/ s/, '$newest'\]/]/"
expect "branch missing from the phpt matrix" 1 "$t" "ci.yml: job phpt"

t=$(make_tree build)
edit "$t" .github/workflows/release.yml "/'$second'/ s/, '$newest'\]/]/"
expect "branch missing from the release build matrix" 1 "$t" "release.yml: job build"

t=$(make_tree bundle)
edit "$t" .github/workflows/release.yml "s/-ne $((count * 4))/-ne $(((count - 1) * 4))/"
expect "wrong expected archive count in bundle" 1 "$t" "expected $((count * 4)) archives"

t=$(make_tree official)
edit "$t" .github/workflows/release.yml "/smoke-official.sh/ s/ $second / /"
expect "branch missing from smoke on official images" 1 "$t" "smoke-official.sh"

t=$(make_tree sury)
edit "$t" .github/workflows/release.yml "/smoke-debian-sury.sh/ s/ $newest\$//"
expect "branch missing from smoke on sury" 1 "$t" "smoke-debian-sury.sh"

t=$(make_tree remi)
edit "$t" .github/workflows/release.yml "/smoke-remi.sh/ s/ $newest\$//"
expect "Remi smoke without the newest branch" 1 "$t" "smoke-remi.sh"

t=$(make_tree readme)
edit "$t" README.md "/^- PHP $oldest, / s/, $newest\$//"
expect "README version line without a branch" 1 "$t" "README.md: supported versions"

t=$(make_tree range)
printf '\nGreen on PHP %s–%s.\n' "$oldest" "$prev" >>"$t/CONTRIBUTING.md"
expect "stale version range in CONTRIBUTING" 1 "$t" "CONTRIBUTING.md: range $oldest–$prev"

t=$(make_tree valgrind)
edit "$t" .github/workflows/ci.yml "s/php:$newest-cli sh -c/php:$prev-cli sh -c/"
expect "valgrind job on an old branch" 1 "$t" "job phpt-valgrind"

t=$(make_tree arm64)
edit "$t" .github/workflows/ci.yml "s/\['$oldest', '$newest'\]/['$oldest', '$prev']/"
expect "aarch64 job without the newest branch" 1 "$t" "job phpt-arm64"

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all check-php-matrix tests passed"
