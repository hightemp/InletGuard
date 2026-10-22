#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Every list of supported PHP branches must match the single source,
# scripts/release/php-versions.txt: CI and release matrices, the expected
# number of release archives, smoke lists, jobs that run on the newest branch,
# the README version line and "7.0–X" ranges in the docs.
#
#   sh scripts/check-php-matrix.sh [source-root]

set -eu

root=${1:-$(cd "$(dirname "$0")/.." && pwd)}
cd "$root"

ci=.github/workflows/ci.yml
rel=.github/workflows/release.yml

all=$(grep -v '^#' scripts/release/php-versions.txt | awk 'NF { printf "%s ", $1 }' | sed 's/ $//')
oldest=${all%% *}
newest=${all##* }
count=$(echo "$all" | wc -w)

errors=0
err()
{
	echo "check-php-matrix: $*"
	errors=$((errors + 1))
}

# job_block <file> <job>: lines of a top-level job (indented by two spaces)
job_block()
{
	awk -v job="  $2:" '
		$0 == job { inside = 1; next }
		inside && /^  [^ ]/ { exit }
		inside { print }
	' "$1"
}

# job_matrix <file> <job>: branches of the first "php: [...]" in the job
job_matrix()
{
	job_block "$1" "$2" | grep -m 1 'php: *\[' | sed "s/.*\[//; s/\].*//; s/[',]/ /g" | xargs
}

# same <what> <expected> <got>
same()
{
	[ "$2" = "$3" ] || err "$1 (expected '$2', got '$3')"
}

# all branches
same "$ci: job phpt matrix" "$all" "$(job_matrix "$ci" phpt)"
same "$rel: job build matrix" "$all" "$(job_matrix "$rel" build)"

# jobs that test the newest branch
for job in phpt-asan phpt-minimal-modules phpt-arm64; do
	case " $(job_matrix "$ci" "$job") " in
	*" $newest "*) ;;
	*) err "$ci: job $job does not run on the newest branch $newest" ;;
	esac
done
for v in $(job_block "$ci" phpt-valgrind | grep -o 'php[: ][0-9][0-9.]*' | sed 's/^php.//' | sort -u); do
	same "$ci: job phpt-valgrind runs on" "$newest" "$v"
done

# release archives: branches x NTS/ZTS x two architectures
# shellcheck disable=SC2016 # a literal "$count" in the workflow
archives=$(sed -n 's/.*"\$count" -ne \([0-9]*\).*/\1/p' "$rel")
same "$rel: bundle expected $((count * 4)) archives" "$((count * 4))" "$archives"

# smoke lists
smoke_lists()
{
	grep "$1" "$rel" | sed "s/.*$1 [^ ]* //" || true
}
n=0
for list in $(smoke_lists smoke-official.sh | tr ' ' ','); do
	same "$rel: smoke-official.sh branches" "$all" "$(echo "$list" | tr ',' ' ')"
	n=$((n + 1))
done
[ "$n" -gt 0 ] || err "$rel: no smoke-official.sh run"
n=0
for list in $(smoke_lists smoke-debian-sury.sh | tr ' ' ','); do
	same "$rel: smoke-debian-sury.sh branches" "$all" "$(echo "$list" | tr ',' ' ')"
	n=$((n + 1))
done
[ "$n" -gt 0 ] || err "$rel: no smoke-debian-sury.sh run"
n=0
for list in $(smoke_lists smoke-remi.sh | tr ' ' ','); do
	list=$(echo "$list" | tr ',' ' ')
	case "$all" in
	*"$list") ;;
	*) err "$rel: smoke-remi.sh branches '$list' are not a tail of '$all' ending at $newest" ;;
	esac
	n=$((n + 1))
done
[ "$n" -gt 0 ] || err "$rel: no smoke-remi.sh run"

# docs
readme=$(sed -n "s/^- PHP \($oldest, .*\)/\1/p" README.md | sed 's/,//g')
same "README.md: supported versions" "$all" "$readme"
for f in README.md CONTRIBUTING.md .github/pull_request_template.md src/zend/php_compat.h; do
	for r in $(grep -oE "$oldest ?(–|-|…) ?[0-9]+\.[0-9]+" "$f" | tr -d ' ' | sort -u); do
		top=$(echo "$r" | grep -oE '[0-9]+\.[0-9]+$')
		[ "$top" = "$newest" ] || err "$f: range $r does not end at the newest branch $newest"
	done
done

if [ "$errors" -ne 0 ]; then
	echo "check-php-matrix: $errors problem(s); the branch list is scripts/release/php-versions.txt"
	exit 1
fi
echo "PHP branch lists are consistent ($oldest-$newest, $count branches)"
