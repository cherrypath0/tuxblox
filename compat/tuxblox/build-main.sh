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

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ProtonDir="${1:?usage: build-main.sh <proton build dir>}"
: "${TUXBLOX_COMPAT_VERSION:?TUXBLOX_COMPAT_VERSION must be set}"

podman build -t tuxblox-old-glibc-builder -f "$ROOT/Containerfile" "$ROOT"

Work="$(mktemp -d)"
trap 'rm -rf "$Work"' EXIT
mkdir -p "$Work/src" "$Work/out"

cp -a "$ROOT"/compat/tuxblox/src/. "$Work/src/"
mkdir -p "$Work/src/shared_paths" "$Work/src/shared_flags" "$Work/src/shared_release"
cp -a "$ROOT"/shared/paths/src/. "$Work/src/shared_paths/"
cp "$ROOT"/shared/flags/FlagManager.cpp "$ROOT"/shared/flags/FlagManager.h "$Work/src/shared_flags/"
cp "$ROOT"/shared/release/src/release_stamp.cpp "$ROOT"/shared/release/src/release_stamp.h "$Work/src/shared_release/"
cp "$ROOT"/shared/crypto/src/ca_bundle.cpp "$ROOT"/shared/crypto/src/ca_bundle.h "$Work/src/shared_flags/"

CaHeader="$ROOT/build/.artifacts/launcher/generated/tuxblox_ca_bundle_pem.h"
if [ ! -f "$CaHeader" ]; then
    echo "missing $CaHeader -- the launcher must build first" >&2
    exit 1
fi
cp "$CaHeader" "$Work/src/"

"$ROOT/compat/tuxblox/tools/embed-data.py" "$ROOT/compat/tuxblox/data" "$Work/src/embedded_data.h"
"$ROOT/shared/flags/tools/embed-flags.py" "$ROOT/shared/flags/DefaultFlags-x86_64.json" \
    "$Work/src/tuxblox_default_flags.h" kTuxBloxDefaultFlagsJson

podman run --rm --userns=keep-id -v "$Work:/work:Z" -w /work/src \
    -v "$ROOT/build/.artifacts/shared:/shared:ro,z" \
    -e TUXBLOX_COMPAT_VERSION \
    tuxblox-old-glibc-builder sh -c 'g++ -std=c++17 -O2 -Wall -Wextra -static-libstdc++ -static-libgcc \
        -I. -Ithird_party -Ishared_flags -Ishared_release -I/shared/include \
        -DTUXBLOX_VERSION="\"$TUXBLOX_COMPAT_VERSION\"" \
        -o /work/out/main $(find . -name "*.cpp") \
        /shared/lib/libcurl.a /shared/lib/libnghttp2.a /shared/lib/libssl.a \
        /shared/lib/libcrypto.a /shared/lib/libz.a -ldl -lpthread'

install -m 755 "$Work/out/main" "$ProtonDir/dist/main"
