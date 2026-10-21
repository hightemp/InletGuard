#!/bin/sh
# SPDX-License-Identifier: MIT
#
# PHP version checks are allowed only in src/zend/php_compat.h (NFR-4).
# Any mention of a version macro elsewhere (code or comment) is an error.
#
#   sh scripts/check-compat.sh [source-root]

set -eu

root=${1:-$(cd "$(dirname "$0")/.." && pwd)}
macros='PHP_VERSION_ID|PHP_MAJOR_VERSION|PHP_MINOR_VERSION|PHP_RELEASE_VERSION|ZEND_MODULE_API_NO|ZEND_EXTENSION_API_NO'

cd "$root"
violations=$(find . -path ./third_party -prune -o -type f \( -name '*.c' -o -name '*.h' \) -print |
	sed 's|^\./||' |
	grep -v '^src/zend/php_compat\.h$' |
	xargs -r grep -nE "$macros" /dev/null || true)

if [ -n "$violations" ]; then
	echo "PHP version checks are only allowed in src/zend/php_compat.h:"
	echo "$violations"
	exit 1
fi
echo "compat checks passed"
