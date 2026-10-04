#!/usr/bin/env bash
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

# Builds the libraries TuxBlox ships inside its own programs, in the same container every one of them is built in.

set -euo pipefail

ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
Root="$(cd "$ScriptDir/../.." && pwd)"
OutDir="$Root/build/.artifacts/shared"
ScratchDir="$Root/build/.artifacts/shared-build"
JOBS="${TUXBLOX_MAKE_JOBS:-$(nproc 2>/dev/null || echo 1)}"
# Lives outside build/, which every full build wipes, so an unchanged pin is a cache hit rather than a recompile
CacheDir="${CCACHE_DIR:-$HOME/.ccache}"

Submodules=(
    "shared/crypto/openssl:OPENSSL_COMMIT"
    "shared/network/libcurl:CURL_COMMIT"
    "shared/network/nghttp2:NGHTTP2_COMMIT"
    "shared/fs/libarchive:LIBARCHIVE_COMMIT"
    "shared/fs/zstd:ZSTD_COMMIT"
    "shared/fs/zlib:ZLIB_COMMIT"
)

CommitArgs=()
for entry in "${Submodules[@]}"; do
    path="${entry%%:*}"
    var="${entry##*:}"
    if [[ ! -e "$Root/$path/.git" ]]; then
        printf '!! %s is empty. Run: git submodule update --init %s\n' "$path" "$path" >&2
        exit 1
    fi
    commit="$(git -C "$Root/$path" rev-parse --short HEAD)"
    CommitArgs+=(-e "$var=$commit")
done

printf ':: Building builder container image (old-glibc baseline)\n'
podman build -t tuxblox-old-glibc-builder -f "$Root/Containerfile" "$Root"

# The container runs as the calling user, so it cannot create anything at the root; both the output and the scratch space are mounted in.
mkdir -p "$OutDir" "$ScratchDir" "$CacheDir"
printf ':: Building the shared libraries (in podman, rootless, old-glibc baseline)\n'
podman run --rm --userns=keep-id -e JOBS="$JOBS" "${CommitArgs[@]}" \
    -v "$Root:/src:ro,z" -v "$OutDir:/out:z" -v "$ScratchDir:/build:z" \
    -v "$CacheDir:/ccache:z" -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G \
    tuxblox-old-glibc-builder bash /src/shared/tools/libs-build-in-container.sh

printf ':: Done. %s\n' "$OutDir"
