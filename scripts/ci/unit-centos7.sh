#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Build and run the core unit tests with the CentOS 7 toolchain (GCC 4.8.5,
# glibc 2.17) - the oldest compiler targeted by hosting environments.
#
#   docker run --rm -v "$PWD:/src:ro" centos:7 sh /src/scripts/ci/unit-centos7.sh

set -eu

# CentOS 7 is EOL; its packages only live on vault.centos.org now.
sed -i -e 's/^mirrorlist=/#mirrorlist=/' \
	-e 's|^#\?baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|' \
	/etc/yum.repos.d/CentOS-*.repo
yum install -q -y gcc make >/dev/null

gcc --version | head -n 1
cp -r /src /tmp/b
make -C /tmp/b/tests/unit CC=gcc
