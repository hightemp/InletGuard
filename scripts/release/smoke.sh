#!/bin/sh
# SPDX-License-Identifier: MIT
# Smoke-test a release archive with a real PHP binary (TASKS 0.5g): pick the
# archive matching that PHP's branch, ZTS mode and architecture, then check
# that the extension loads with the expected version, both directly and
# through the packaged 20-inletguard.ini.
#
#   smoke.sh <php-binary> <archive-dir>

set -eu

php=$1 dir=$2
arch=${SMOKE_ARCH:-$(uname -m)}

info=$("$php" -n -r 'echo PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION, " ", PHP_ZTS ? "zts" : "nts";')
branch=${info% *} variant=${info#* }
label="PHP $branch $variant $arch ($php)"

set -- "$dir"/inletguard-*-php"$branch-$variant-$arch"-linux-gnu.tar.gz
if [ ! -f "$1" ]; then
	echo "FAIL $label: no archive for this PHP in $dir"
	exit 1
fi
if [ $# -gt 1 ]; then
	echo "FAIL $label: more than one matching archive: $*"
	exit 1
fi
archive=$1
base=$(basename "$archive" .tar.gz)
version=${base#inletguard-}
version=${version%-php*}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
tar -C "$work" -xzf "$archive"
ext="$work/$base"

got=$("$php" -n -d extension="$ext/inletguard.so" -r 'echo InletGuard\version();' 2>&1) || true
if [ "$got" != "$version" ]; then
	echo "FAIL $label: direct load reported '$got', expected $version"
	exit 1
fi

got=$(PHP_INI_SCAN_DIR= "$php" -c "$ext/20-inletguard.ini" -d extension_dir="$ext" \
	-r 'echo InletGuard\version();' 2>&1) || true
if [ "$got" != "$version" ]; then
	echo "FAIL $label: load via packaged 20-inletguard.ini reported '$got', expected $version"
	exit 1
fi

echo "PASS $label: $base"
