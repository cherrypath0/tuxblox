#!/bin/bash
# TuxBlox - Linux Compatibility Layer for the Roblox Engine
# Copyright (C) 2026 TuxBlox Developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# shared/tools/build-memory-guard.sh
#
# Stops the build containers when memory runs low, so the kernel does not get to pick the
# victim itself. A WebKit build at too many jobs will take the desktop down with it.
#
# Measured against MemAvailable rather than "used", because that is the number that says
# what can still be allocated. Cache counts as available and is not pressure.
#
# Usage:
#   build-memory-guard.sh                 # watch, stop the build if memory runs out
#   build-memory-guard.sh --dry-run       # report and never stop anything
#   build-memory-guard.sh --limit-gb 26   # a different ceiling
set -uo pipefail

LimitGb=28
Interval=5
DryRun=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --limit-gb) LimitGb="${2:?--limit-gb needs a number}"; shift 2 ;;
        --interval) Interval="${2:?--interval needs seconds}"; shift 2 ;;
        --dry-run) DryRun=1; shift ;;
        *) echo "Usage: $0 [--limit-gb N] [--interval S] [--dry-run]" >&2; exit 2 ;;
    esac
done

meminfoKb() { awk -v k="$1" '$1 == k":" { print $2; exit }' /proc/meminfo; }

TotalKb="$(meminfoKb MemTotal)"
LimitKb=$(( LimitGb * 1024 * 1024 ))

printf ':: Watching: stop builds when used exceeds %sGB of %sGB, checking every %ss\n' \
    "$LimitGb" "$(( TotalKb / 1024 / 1024 ))" "$Interval"
[ "$DryRun" -eq 1 ] && printf ':: Dry run, nothing will be stopped\n'

# Only TuxBlox's own builder images, so nothing else a person is running gets caught.
buildContainers() {
    podman ps --format '{{.ID}} {{.Image}}' 2>/dev/null |
        awk '$2 ~ /tuxblox-(webkitgtk-builder|ui-stack-builder|old-glibc-builder)/ { print $1 }'
}

SeenBuild=0

while true; do
    if [ -n "$(buildContainers)" ]; then
        SeenBuild=1
    elif [ "$SeenBuild" -eq 1 ]; then
        printf ':: Build finished, nothing left to watch\n'
        exit 0
    fi

    AvailKb="$(meminfoKb MemAvailable)"
    UsedKb=$(( TotalKb - AvailKb ))

    if [ "$UsedKb" -ge "$LimitKb" ]; then
        printf '!! %s  used %sGB of %sGB -- over the %sGB ceiling\n' \
            "$(date +%H:%M:%S)" "$(( UsedKb / 1024 / 1024 ))" \
            "$(( TotalKb / 1024 / 1024 ))" "$LimitGb" >&2

        mapfile -t containers < <(buildContainers)
        if [ "${#containers[@]}" -eq 0 ]; then
            printf '   no TuxBlox build container running, nothing to stop\n' >&2
            exit 0
        fi
        if [ "$DryRun" -eq 1 ]; then
            printf '   would stop: %s\n' "${containers[*]}" >&2
            exit 0
        fi
        printf '   stopping: %s\n' "${containers[*]}" >&2
        podman kill "${containers[@]}" >/dev/null 2>&1
        printf '   stopped. Rerun the build with fewer jobs (the bundle defaults to JOBS=4).\n' >&2
        exit 1
    fi

    sleep "$Interval"
done
