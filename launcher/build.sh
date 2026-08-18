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
# TUXBLOX_SKIP_DEPS is set by the root build.sh, which installs dependencies
# once for all three builds -- avoids repeated package-manager round trips.
if [[ -n "$TUXBLOX_SKIP_DEPS" ]]; then
    echo ":: TUXBLOX_SKIP_DEPS set, skipping dependency install"
else
    install_deps
fi

echo ":: Vendoring third-party sources"
./vendor.sh

echo ":: Building builder container image (old-glibc baseline)"
podman build -t tuxblox-old-glibc-builder -f ../Containerfile ..

# A build/ configured outside the container records host paths in CMakeCache.txt;
# cmake hard-errors if that cache is reused from /src/build inside the container.
if [[ -f build/CMakeCache.txt ]] && \
   ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$' build/CMakeCache.txt; then
    echo ":: Dropping stale host-configured build/ (not configured inside the container)"
    rm -rf build
fi

echo ":: Configuring + Building (in podman, rootless, old-glibc baseline)"
# EmbedLicense.cmake embeds the repo-root LICENSE via /src/../LICENSE, which
# resolves to /LICENSE inside the container -- mount it there read-only, since
# only launcher/ itself is mounted at /src.
#
# bundle-qt.sh is chained into the SAME container invocation, not run afterwards
# on the host: it copies Qt6 out of /opt/qt6/6.6.3/gcc_64, a path that only
# exists inside this image (see Containerfile -- Ubuntu 20.04 has no Qt6 apt
# packages, so aqtinstall puts it there). Without it the produced binary keeps a
# RUNPATH into that container-only path and cannot start on any machine without
# a coincidentally-present system Qt6.
# TUXBLOX_BUILD_VERSION has to be forwarded explicitly: cmake runs INSIDE this
# container, so an env var exported by the root build.sh on the host is invisible
# to it otherwise, and CMakeLists.txt would silently fall back to the VERSION file.
# Empty when this script is run standalone, which is exactly the fallback case.
podman run --rm --userns=keep-id -e JOBS="$JOBS" \
    -e TUXBLOX_BUILD_VERSION="${TUXBLOX_BUILD_VERSION:-}" -v "$(pwd):/src:Z" \
    -v "$(pwd)/../LICENSE:/LICENSE:ro,z" -w /src tuxblox-old-glibc-builder \
    bash -c 'cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$JOBS" && ./bundle-qt.sh'

# Also stage the finished binary + its Qt6 bundle at the repo-root build/
# directory -- the same place the root build.sh (which stages this whole
# build/ tree there via `mv` after calling this script) leaves it, so a
# standalone run of this script produces a runnable artifact in the same
# place either way. Plain `cp`, not `mv`: this script's own build/ must stay
# intact for incremental rebuilds. libtuxblox/ has to be re-copied wholesale
# (not merged) since the binary's RPATH ($ORIGIN/libtuxblox/lib) requires the
# two to stay exact siblings -- a stale bundle left over from a previous copy
# could silently mismatch a freshly rebuilt binary.
mkdir -p ../build
cp -f build/TuxBloxLauncher ../build/TuxBloxLauncher
rm -rf ../build/libtuxblox
cp -a build/libtuxblox ../build/libtuxblox

echo ":: Done. Binary at build/TuxBloxLauncher (Qt6 bundled beside it in build/libtuxblox/)"
echo ":: Also staged to $(cd .. && pwd)/build/TuxBloxLauncher (+ libtuxblox/)"
