#!/bin/sh
# SPDX-License-Identifier: MIT
# Run a command up to <attempts> times, sleeping <delay> seconds between
# attempts. Exits with the status of the last attempt. Meant for network
# steps that depend on external repositories, never for the checks themselves.
#
#   retry.sh <attempts> <delay-seconds> <command> [args...]

set -u

attempts=$1 delay=$2
shift 2

i=1
while :; do
	"$@"
	status=$?
	[ "$status" -eq 0 ] && exit 0
	if [ "$i" -ge "$attempts" ]; then
		echo "retry: '$*' failed after $i attempts (exit $status)" >&2
		exit "$status"
	fi
	echo "retry: attempt $i of $attempts failed (exit $status), retrying in ${delay}s: $*" >&2
	i=$((i + 1))
	sleep "$delay"
done
