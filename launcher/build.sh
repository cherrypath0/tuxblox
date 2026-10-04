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
# Lives outside build/, which every full build wipes, so a rebuild of unchanged sources is a cache hit
CacheDir="${CCACHE_DIR:-$HOME/.ccache}"

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

# Built here when absent so this script still works on its own; the root build.sh runs it once up front.
if [[ ! -f ../build/.artifacts/shared/lib/libcurl.a ]]; then
    printf ':: Shared libraries missing, building them first\n'
    ../shared/tools/libs-build.sh
fi

mkdir -p "$CacheDir"
echo ":: Building builder container image (old-glibc baseline)"
podman build -t tuxblox-old-glibc-builder -f ../Containerfile ..

# A build/ configured outside the container records host paths in CMakeCache.txt;
# cmake hard-errors if that cache is reused from /src/build inside the container.
if [[ -f build/CMakeCache.txt ]] && \
   ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$' build/CMakeCache.txt; then
    echo ":: Dropping stale host-configured build/ (not configured inside the container)"
    rm -rf build
fi

# cmake also hard-errors when the cached generator is not the one being asked for, which is what a build/ from before the Ninja switch holds.
if [[ -f build/CMakeCache.txt ]] && \
   ! grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja$' build/CMakeCache.txt; then
    echo ":: Dropping stale build/ (configured for a different generator)"
    rm -rf build
fi

echo ":: Configuring + Building (in podman, rootless, old-glibc baseline)"
# EmbedLicense.cmake embeds the repo-root LICENSE via /src/../LICENSE, which
# resolves to /LICENSE inside the container -- mount it there read-only, since
# only launcher/ itself is mounted at /src.
#
# TUXBLOX_BUILD_VERSION has to be forwarded explicitly: cmake runs INSIDE this
# container, so an env var exported by the root build.sh on the host is invisible
# to it otherwise.
#
# The repo-root VERSION file cannot cover that fallback by itself, because only
# launcher/ is mounted at /src -- the root of the repo is not reachable from
# inside the container at all. So a standalone run of this script (no root
# build.sh, hence no env var) reads VERSION here on the HOST and passes the
# value in through the same variable, which is why one number reaches the
# launcher, the installer and the compatibility layer either way.
if [[ -z "${TUXBLOX_BUILD_VERSION:-}" && -r "$(pwd)/../VERSION" ]]; then
    TUXBLOX_BUILD_VERSION="$(sed -n '1p' "$(pwd)/../VERSION" | tr -d '[:space:]')"
fi

# The channel rides along for the same reason, from line 2 of the same file.
if [[ -z "${TUXBLOX_CHANNEL:-}" && -r "$(pwd)/../VERSION" ]]; then
    TUXBLOX_CHANNEL="$(sed -n '2p' "$(pwd)/../VERSION" | tr -d '[:space:]')"
fi

# The notice names the libadwaita commit the interface was built from, because TuxBlox ships a modified fork and a recipient has to be able to fetch that exact source. git is not usable inside the container, so it is read here. Every release installs the stack, so even a headless installer names it; it names none only when there is no stack to name.
# The empty default is deliberate: CMake caches the variable, so a build without an interface must clear a commit left by an earlier one.
LibadwaitaArg="-DTUXBLOX_LIBADWAITA_COMMIT="
source "$(pwd)/../shared/tools/ui-libadwaita-commit.sh"
LibadwaitaCommit="$(libadwaitaCommit "$(pwd)/../shared/ui")"
if [[ -n "$LibadwaitaCommit" ]]; then
    if [[ "$LibadwaitaCommit" == *-dirty ]]; then
        echo "!! The libadwaita the interface was built from has uncommitted changes: the notice will name $LibadwaitaCommit, a modified tree that is not a published commit. Do not release this build." >&2
    fi
    LibadwaitaArg="-DTUXBLOX_LIBADWAITA_COMMIT=$LibadwaitaCommit"
fi

# The interface stack and the sources shared with the other programs live in shared/ui, outside this script's /src mount, so each gets a mount of its own
UiStackDev="$(pwd)/../shared/ui/dist/dev"
UiSrc="$(pwd)/../shared/ui/src"
if [[ ! -d "$UiStackDev" ]]; then
    printf '!! shared/ui/dist/dev is missing. Run shared/tools/ui-build.sh first.\n' >&2
    exit 1
fi

shopt -s nullglob
StackTarballs=("$(pwd)"/../shared/ui/dist/ui-stack-*.tar.zst)
shopt -u nullglob
if [[ ${#StackTarballs[@]} -ne 1 ]]; then
    printf '!! Expected exactly one interface stack tarball in shared/ui/dist/, found %s.\n' "${#StackTarballs[@]}" >&2
    exit 1
fi

podman run --rm --userns=keep-id -e JOBS="$JOBS" -e LIBADWAITA_ARG="$LibadwaitaArg" \
    -e TUXBLOX_BUILD_VERSION="${TUXBLOX_BUILD_VERSION:-}" \
    -e TUXBLOX_CHANNEL="${TUXBLOX_CHANNEL:-}" -v "$(pwd):/src:Z" \
    -v "$(pwd)/../LICENSE:/LICENSE:ro,z" -v "$(cd "$UiSrc" && pwd):/ui-src:ro" -v "$(cd "$UiStackDev" && pwd):/ui-dev:ro" -v "$(pwd)/../build/.artifacts/shared:/shared:ro,z" -v "$(pwd)/../shared/crypto:/shared-crypto:ro,z" -v "$CacheDir:/ccache:z" -e CCACHE_DIR=/ccache -w /src tuxblox-old-glibc-builder \
    bash -c 'cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DTUXBLOX_UI_SRC=/ui-src -DTUXBLOX_UI_STACK_DEV=/ui-dev -DTUXBLOX_SHARED_DIR=/shared -DTUXBLOX_SHARED_CRYPTO=/shared-crypto/src ${LIBADWAITA_ARG:-} && cmake --build build -j"$JOBS"'

if [[ ! -f build/TuxBloxLauncher ]]; then
    printf '!! build/TuxBloxLauncher was not built. The window needs shared/ui/dist/dev.\n' >&2
    exit 1
fi

# A failure here stops the build before anything is staged: the finished binary has to start on a machine with no GTK. TUXBLOX_SKIP_SMOKE=1 is the opt-out for iterating, never for a release.
if [[ "${TUXBLOX_SKIP_SMOKE:-}" == "1" ]]; then
    printf '!! TUXBLOX_SKIP_SMOKE set, the fresh-install smoke test was NOT run\n' >&2
else
    ./smoke-test.sh build/TuxBloxLauncher "${StackTarballs[0]}"
fi

# Also stage the finished binary and the interface libraries beside it at the repo-root build/, where the root build.sh leaves them too. libtuxblox/ is re-extracted wholesale, since the binary finds its libraries beside itself and a stale mix would not.
mkdir -p ../build
cp -f build/TuxBloxLauncher ../build/TuxBloxLauncher
rm -rf ../build/libtuxblox
mkdir -p ../build/libtuxblox
tar --zstd -xf "${StackTarballs[0]}" -C ../build/libtuxblox

echo ":: Done. Also staged to $(cd .. && pwd)/build/TuxBloxLauncher (+ libtuxblox/)"
