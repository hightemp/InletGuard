#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for scripts/release/smoke.sh with a stub "php" and fake archives.
# Real loading of the .so files is exercised by the release workflow.

set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SMOKE="$ROOT/scripts/release/smoke.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

failures=0

# Stub php: reports STUB_BRANCH/STUB_VARIANT, and "loads" an extension by
# printing the version stored in the fake .so (or STUB_VERSION if set).
# Every invocation is logged to $STUB_LOG.
cat >"$TMP/php" <<'EOF'
#!/bin/sh
echo "$*" >>"$STUB_LOG"
ext="" extdir="" ini="" code=""
while [ $# -gt 0 ]; do
	case "$1" in
	-n) ;;
	-c) ini=$2; shift ;;
	-d)
		case "$2" in
		extension=*) ext=${2#extension=} ;;
		extension_dir=*) extdir=${2#extension_dir=} ;;
		esac
		shift
		;;
	-r) code=$2; shift ;;
	esac
	shift
done
case "$code" in
*PHP_ZTS*) printf '%s %s' "$STUB_BRANCH" "$STUB_VARIANT"; exit 0 ;;
esac
if [ -z "$ext" ] && [ -n "$ini" ] && grep -q '^extension=inletguard.so' "$ini"; then
	ext="$extdir/inletguard.so"
fi
[ -f "$ext" ] || { echo "PHP Warning: Unable to load dynamic library '$ext'"; exit 1; }
printf '%s' "${STUB_VERSION:-$(cat "$ext")}"
EOF
chmod +x "$TMP/php"

# fake_archive <dir> <version> <branch> <variant> <arch>
fake_archive()
{
	base="inletguard-$2-php$3-$4-$5-linux-gnu"
	mkdir -p "$TMP/stage/$base" "$1"
	printf '%s' "$2" >"$TMP/stage/$base/inletguard.so"
	printf 'extension=inletguard.so\n' >"$TMP/stage/$base/20-inletguard.ini"
	tar -C "$TMP/stage" -czf "$1/$base.tar.gz" "$base"
	rm -rf "$TMP/stage"
}

# run <description> <archive-dir> <branch> <variant> <expected exit> [pattern]
run()
{
	: >"$TMP/log"
	out=$(STUB_LOG="$TMP/log" STUB_BRANCH=$3 STUB_VARIANT=$4 SMOKE_ARCH=x86_64 \
		sh "$SMOKE" "$TMP/php" "$2" 2>&1)
	status=$?
	if [ "$status" -ne "$5" ]; then
		echo "FAIL $1: exit $status, output: $out"
		failures=$((failures + 1))
	elif [ -n "${6:-}" ] && ! printf '%s' "$out" | grep -q -- "$6"; then
		echo "FAIL $1: output does not mention '$6': $out"
		failures=$((failures + 1))
	else
		echo "PASS $1"
	fi
}

all="$TMP/all"
for b in 7.0 8.4; do
	for v in nts zts; do
		for a in x86_64 aarch64; do
			fake_archive "$all" 1.2.3 $b $v $a
		done
	done
done

run "picks the archive of its branch, variant and arch" "$all" 8.4 zts 0 "php8.4-zts-x86_64"
if grep -q -- "-c .*20-inletguard.ini" "$TMP/log"; then
	echo "PASS the packaged 20-inletguard.ini is exercised"
else
	echo "FAIL the packaged ini was not used: $(cat "$TMP/log")"
	failures=$((failures + 1))
fi

run "no archive for this PHP branch" "$all" 8.1 nts 1 "no archive"

dup="$TMP/dup"
fake_archive "$dup" 1.2.3 7.0 nts x86_64
fake_archive "$dup" 1.2.4 7.0 nts x86_64
run "two matching archives" "$dup" 7.0 nts 1 "more than one"

out=$(STUB_LOG="$TMP/log" STUB_BRANCH=7.0 STUB_VARIANT=nts SMOKE_ARCH=x86_64 STUB_VERSION=9.9.9 \
	sh "$SMOKE" "$TMP/php" "$all" 2>&1)
if [ $? -ne 0 ] && printf '%s' "$out" | grep -q "9.9.9"; then
	echo "PASS a module reporting another version fails"
else
	echo "FAIL wrong version not detected: $out"
	failures=$((failures + 1))
fi

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all smoke script tests passed"
