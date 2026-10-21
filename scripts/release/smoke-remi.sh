#!/bin/sh
# SPDX-License-Identifier: MIT
# Smoke-test archives against PHP branches from Remi's module streams.
# Runs inside an almalinux image (8 or 9).
#
#   smoke-remi.sh <archive-dir> <branch>...

set -eu

dir=$1
shift
el=$(rpm -E %rhel)
dnf -q -y install "https://rpms.remirepo.net/enterprise/remi-release-$el.rpm" >/dev/null

status=0
for b in "$@"; do
	dnf -q -y module reset php >/dev/null
	dnf -q -y module enable "php:remi-$b" >/dev/null
	dnf -q -y install php-cli >/dev/null
	sh "$(dirname "$0")/smoke.sh" /usr/bin/php "$dir" || status=1
	dnf -q -y remove php-cli php-common >/dev/null
done
exit "$status"
