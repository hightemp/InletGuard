#!/bin/sh
# SPDX-License-Identifier: MIT
# Smoke-test archives against every PHP branch from packages.sury.org (the
# canonical home of the ondrej/php packages). Runs inside a debian image.
#
#   smoke-debian-sury.sh <archive-dir> <branch>...

set -eu

dir=$1
shift
export DEBIAN_FRONTEND=noninteractive
echo 'Acquire::Retries "5";' >/etc/apt/apt.conf.d/80-retries
apt-get update -qq >/dev/null
apt-get install -y -qq --no-install-recommends ca-certificates curl lsb-release >/dev/null
curl -fsSL --retry 3 -o /tmp/sury-keyring.deb https://packages.sury.org/debsuryorg-archive-keyring.deb
dpkg -i /tmp/sury-keyring.deb >/dev/null
echo "deb [signed-by=/usr/share/keyrings/debsuryorg-archive-keyring.gpg] https://packages.sury.org/php/ $(lsb_release -sc) main" \
	>/etc/apt/sources.list.d/sury-php.list
apt-get update -qq >/dev/null
pkgs=""
for b in "$@"; do
	pkgs="$pkgs php$b-cli"
done
# shellcheck disable=SC2086
apt-get install -y -qq --no-install-recommends $pkgs >/dev/null

status=0
for b in "$@"; do
	sh "$(dirname "$0")/smoke.sh" "/usr/bin/php$b" "$dir" || status=1
done
exit "$status"
