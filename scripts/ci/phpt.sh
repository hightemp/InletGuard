#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Build the extension and run the phpt suite. Meant to run inside an official
# php:<version>-<cli|zts> image, both in CI and locally:
#
#   docker run --rm -v "$PWD:/src:ro" php:8.5-zts sh /src/scripts/ci/phpt.sh
#   docker run --rm -v "$PWD:/src:ro" php:8.5-cli sh /src/scripts/ci/phpt.sh --asan
#   PATH=/opt/php-asan/bin:$PATH SRC=$PWD sh scripts/ci/phpt.sh --asan-native
#
# Modes:
#   (none)         regular build with -Werror
#   --asan         sanitizers for a stock (non-ASan) PHP: dlopen shim + LD_PRELOAD
#   --asan-native  sanitizers for a PHP built with -fsanitize=address
#   --valgrind     run-tests -m
#
# Extra configure arguments: INLETGUARD_CONFIGURE, e.g.
#   INLETGUARD_CONFIGURE=--with-inletguard-modules=sqli

set -eu

SRC=${SRC:-/src}
BUILD=${BUILD:-/tmp/inletguard-build}
MODE=${1:-}

rm -rf "$BUILD"
cp -r "$SRC" "$BUILD"
cd "$BUILD"
# A local checkout may contain in-tree build output with host paths.
make distclean >/dev/null 2>&1 || true
phpize --clean >/dev/null 2>&1 || true
find . -name '*.dep' -o -name '*.lo' -o -name '*.o' | xargs rm -f

php -r 'echo "PHP ", PHP_VERSION, (PHP_ZTS ? " ZTS" : " NTS"), PHP_EOL;'

CFLAGS="-g -O2"
LDFLAGS=""
DEV="--enable-inletguard-dev"
if [ "$MODE" = "--asan" ] || [ "$MODE" = "--asan-native" ]; then
	CFLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined"
	LDFLAGS="-fsanitize=address,undefined"
	# Some PHP headers redefine __SANITIZE_ADDRESS__ under newer GCC, a warning
	# that cannot be disabled; -Werror is enforced by the regular jobs instead.
	DEV=""
	# LeakSanitizer does not work in unprivileged containers and would make
	# configure's test programs fail; leaks are covered by the valgrind job.
	export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1:verify_asan_link_order=0"
	export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
fi
if [ "$MODE" = "--asan" ]; then
	# Stock PHP loads extensions with RTLD_DEEPBIND, which the sanitizer
	# runtime rejects. Strip the flag with a tiny dlopen() interposer instead
	# of rebuilding PHP itself with ASan.
	cat >/tmp/nodeepbind.c <<'SHIM'
#define _GNU_SOURCE
#include <dlfcn.h>
void *dlopen(const char *file, int mode)
{
	static void *(*real_dlopen)(const char *, int);
	if (!real_dlopen) {
		real_dlopen = (void *(*)(const char *, int))dlsym(RTLD_NEXT, "dlopen");
	}
	return real_dlopen(file, mode & ~RTLD_DEEPBIND);
}
SHIM
	gcc -shared -fPIC -o /tmp/nodeepbind.so /tmp/nodeepbind.c -ldl
fi

phpize >/dev/null
if [ "$MODE" = "--asan" ] || [ "$MODE" = "--asan-native" ]; then
	# run-tests.php of PHP 7.0-7.3 forces USE_ZEND_ALLOC=1 for test processes
	# unless -m is given; with the Zend allocator ASan cannot see overflows
	# inside its chunks. Keep the system allocator.
	sed -i "s/\$env\['USE_ZEND_ALLOC'\] = '1';/\$env['USE_ZEND_ALLOC'] = '0';/" run-tests.php
fi
# shellcheck disable=SC2086
./configure --enable-inletguard $DEV ${INLETGUARD_CONFIGURE:-} CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" >/dev/null
make -j"$(nproc)" >/dev/null

export NO_INTERACTION=1
export REPORT_EXIT_STATUS=1
TESTS="--show-diff tests"

case "$MODE" in
--asan)
	# The PHP binary itself is not instrumented, so the ASan runtime must be
	# preloaded; the Zend allocator is disabled so ASan sees every allocation.
	export USE_ZEND_ALLOC=0
	LD_PRELOAD="/tmp/nodeepbind.so $(gcc -print-file-name=libasan.so)" make test TESTS="$TESTS"
	;;
--asan-native)
	# The ASan runtime is already linked into PHP, which then loads
	# extensions without RTLD_DEEPBIND.
	export USE_ZEND_ALLOC=0
	make test TESTS="$TESTS"
	;;
--valgrind)
	make test TESTS="-m $TESTS"
	;;
"")
	make test TESTS="$TESTS"
	;;
*)
	echo "unknown mode: $MODE" >&2
	exit 2
	;;
esac
