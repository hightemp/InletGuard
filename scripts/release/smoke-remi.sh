#!/bin/sh
# SPDX-License-Identifier: MIT
# Smoke-test archives against PHP branches from Remi's module streams.
# Runs inside an almalinux image (8 or 9).
#
#   smoke-remi.sh <archive-dir> <branch>...

set -eu

dir=$1
shift
here=$(dirname "$0")
el=$(rpm -E %rhel)
# Mirrors (EPEL in particular) fail now and then: retry network steps.
printf 'retries=10\ntimeout=60\n' >>/etc/dnf/dnf.conf
retry()
{
	sh "$here/retry.sh" 5 30 "$@"
}
retry dnf -q -y install "https://rpms.remirepo.net/enterprise/remi-release-$el.rpm" >/dev/null

status=0
for b in "$@"; do
	dnf -q -y module reset php >/dev/null
	retry dnf -q -y module enable "php:remi-$b" >/dev/null
	retry dnf -q -y install php-cli >/dev/null
	sh "$here/smoke.sh" /usr/bin/php "$dir" || status=1
	dnf -q -y remove php-cli php-common >/dev/null
done
exit "$status"
