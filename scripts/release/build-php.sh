#!/bin/sh
# SPDX-License-Identifier: MIT
# Build a minimal PHP (headers, phpize, php-config, CLI, CGI) from a verified
# source tarball. Runs inside the manylinux2014 build image.
#
#   build-php.sh <tarball> <nts|zts> <prefix>
#
# EXTRA_CFLAGS / EXTRA_CONFIGURE are appended (used for sanitizer builds).

set -eu

tarball=$1 variant=$2 prefix=$3
work=$(mktemp -d)
tar -C "$work" -xJf "$tarball"
cd "$work"/php-*

version=$(basename "$tarball" .tar.xz)
version=${version#php-}
zts=""
if [ "$variant" = "zts" ]; then
	case "$version" in
	7.*) zts="--enable-maintainer-zts" ;;
	*) zts="--enable-zts" ;;
	esac
fi

# gcc >= 10 defaults to -fno-common, which breaks the build of old PHP 7.x.
log="$work/build.log"
run()
{
	"$@" >>"$log" 2>&1 || {
		tail -n 60 "$log" >&2
		echo "failed: $*" >&2
		exit 1
	}
}

# shellcheck disable=SC2086
run env CFLAGS="-O2 -fcommon ${EXTRA_CFLAGS:-}" ./configure --prefix="$prefix" \
	--disable-all --without-pear --disable-phpdbg $zts ${EXTRA_CONFIGURE:-}
run make -j"$(nproc)"
run make install
"$prefix/bin/php" -r 'echo "built PHP ", PHP_VERSION, (PHP_ZTS ? " ZTS" : " NTS"), PHP_EOL;'
rm -rf "$work"
