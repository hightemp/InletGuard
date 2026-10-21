#!/bin/sh
# SPDX-License-Identifier: MIT
# Build the extension against a PHP prefix, run the phpt suite with that PHP
# and package the result. Runs inside the manylinux2014 build image.
# Prints the archive path.
#
#   build-ext.sh <php-prefix> <php-branch> <nts|zts> <arch> <out-dir>

set -eu

prefix=$1 branch=$2 variant=$3 arch=$4 out=$5
root=$(cd "$(dirname "$0")/../.." && pwd)

work=$(mktemp -d)
cp -r "$root" "$work/src"
cd "$work/src"
make distclean >/dev/null 2>&1 || true
"$prefix/bin/phpize" --clean >/dev/null 2>&1 || true

"$prefix/bin/phpize" >/dev/null
./configure --with-php-config="$prefix/bin/php-config" --enable-inletguard >/dev/null
make -j"$(nproc)" >/dev/null
strip --strip-unneeded modules/inletguard.so

NO_INTERACTION=1 REPORT_EXIT_STATUS=1 TEST_PHP_EXECUTABLE="$prefix/bin/php" \
	make test TESTS="--show-diff tests" >&2

"$prefix/bin/php" -n -d extension="$PWD/modules/inletguard.so" \
	-r 'echo "loaded InletGuard ", InletGuard\version(), PHP_EOL;' >&2

sh "$root/scripts/release/package.sh" modules/inletguard.so "$branch" "$variant" "$arch" "$out"
rm -rf "$work"
