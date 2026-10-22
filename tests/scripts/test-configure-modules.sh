#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Tests for --with-inletguard-modules in config.m4. Needs phpize, so it runs
# inside a php image:
#
#   docker run --rm -v "$PWD:/src:ro" php:8.5-cli sh /src/tests/scripts/test-configure-modules.sh

set -u

SRC=${SRC:-/src}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cp -r "$SRC" "$TMP/b"
cd "$TMP/b"
make distclean >/dev/null 2>&1 || true
phpize --clean >/dev/null 2>&1 || true
phpize >/dev/null 2>&1

failures=0

# accept <value> <expected IG_COMPILED_MODULES>
accept()
{
	if ./configure --enable-inletguard --with-inletguard-modules="$1" >"$TMP/log" 2>&1; then
		got=$(sed -n 's/^#define IG_COMPILED_MODULES "\(.*\)"$/\1/p' config.h)
		if [ "$got" = "$2" ]; then
			echo "PASS '$1' -> '$2'"
			return
		fi
		echo "FAIL '$1': compiled modules '$got', expected '$2'"
	else
		echo "FAIL '$1': configure failed: $(tail -n 2 "$TMP/log")"
	fi
	failures=$((failures + 1))
}

# reject <value>
reject()
{
	if ./configure --enable-inletguard --with-inletguard-modules="$1" >"$TMP/log" 2>&1; then
		echo "FAIL '$1': configure accepted it"
		failures=$((failures + 1))
	elif grep -q "known modules" "$TMP/log"; then
		echo "PASS '$1' rejected with the list of known modules"
	else
		echo "FAIL '$1': rejected without explanation: $(tail -n 2 "$TMP/log")"
		failures=$((failures + 1))
	fi
}

accept all "sqli, xss"
accept "sqli,xss" "sqli, xss"
accept "xss,sqli" "sqli, xss"
accept sqli "sqli"
accept "all,-xss" "sqli"
accept none "none"
reject bogus
reject "sqli,bogus"
reject "all,-bogus"
reject ""

if [ "$failures" -ne 0 ]; then
	echo "$failures test(s) failed"
	exit 1
fi
echo "all configure module tests passed"
