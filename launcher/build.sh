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

set -eo pipefail
cd "$(dirname "$0")"

JOBS="${TUXBLOX_MAKE_JOBS:-$(nproc 2>/dev/null || echo 1)}"

detect_pkg_manager() {
    if command -v apt-get >/dev/null 2>&1; then echo apt
    elif command -v dnf >/dev/null 2>&1; then echo dnf
    elif command -v pacman >/dev/null 2>&1; then echo pacman
    elif command -v brew >/dev/null 2>&1; then echo brew
    elif command -v apk >/dev/null 2>&1; then echo apk
    else echo unknown
    fi
}

install_deps() {
    local mgr
    mgr="$(detect_pkg_manager)"
    case "$mgr" in
        apt)
            sudo apt-get update
            # uidmap provides newuidmap/newgidmap, required for --userns=keep-id;
            # it's only an apt Recommends of podman, so --no-install-recommends hosts miss it.
            sudo apt-get install -y podman curl git uidmap
            ;;
        dnf)
            sudo dnf install -y podman curl git
            ;;
        pacman)
            sudo pacman -S --needed --noconfirm podman curl git
            ;;
        brew)
            brew install podman curl git
            ;;
        apk)
            # uidmap provides newuidmap/newgidmap, required for --userns=keep-id.
            sudo apk add podman curl git uidmap
            ;;
        *)
            echo "!! Unknown package manager. Install manually: podman, curl, git" >&2
            ;;
    esac
}

echo ":: Checking build dependencies"
install_deps

echo ":: Vendoring third-party sources"
./vendor.sh

echo ":: Building builder container image (old-glibc baseline)"
podman build -t tuxblox-old-glibc-builder -f ../build-container/Containerfile ../build-container

# A build/ configured outside the container records host paths in CMakeCache.txt;
# cmake hard-errors if that cache is reused from /src/build inside the container.
if [[ -f build/CMakeCache.txt ]] && \
   ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$' build/CMakeCache.txt; then
    echo ":: Dropping stale host-configured build/ (not configured inside the container)"
    rm -rf build
fi

echo ":: Configuring + Building (in podman, rootless, old-glibc baseline)"
podman run --rm --userns=keep-id -e JOBS="$JOBS" -v "$(pwd):/src:Z" -w /src tuxblox-old-glibc-builder \
    bash -c 'cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$JOBS"'

echo ":: Done. Binary at build/TuxBloxLauncher"
