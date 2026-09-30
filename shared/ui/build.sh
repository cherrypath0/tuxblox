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

set -euo pipefail
cd "$(dirname "$0")"

StackDir="$(pwd)"
DistDir="$StackDir/dist"
ImageTag=tuxblox-ui-stack-builder

# A submodule that was never initialised is an empty folder, and the build would fail deep inside meson with nothing pointing back here
if [ ! -e "$StackDir/libadwaita/meson.build" ]; then
    printf 'ERROR: shared/ui/libadwaita is empty. Run: git submodule update --init shared/ui/libadwaita\n' >&2
    exit 1
fi

LibadwaitaSource="$(git -C "$StackDir/libadwaita" rev-parse --short HEAD)"
if [ -n "$(git -C "$StackDir/libadwaita" status --porcelain)" ]; then
    LibadwaitaSource="$LibadwaitaSource-dirty"
fi

LibadwaitaCommit="$(git -C "$StackDir/libadwaita" rev-parse HEAD)"
if [ -n "$(git -C "$StackDir/libadwaita" status --porcelain)" ]; then
    LibadwaitaCommit="$LibadwaitaCommit-dirty"
fi

mkdir -p "$DistDir/prefix" "$DistDir/work" "$DistDir/out"

printf ':: Building the builder image\n'
podman build -t "$ImageTag" -f Containerfile .

printf ':: Building the stack (a rerun skips every library that already finished)\n'
podman run --rm --userns=keep-id \
    -v "$StackDir:/src:ro" \
    -v "$DistDir/prefix:/opt/tuxblox-ui" \
    -v "$DistDir/work:/build" \
    -e JOBS="${JOBS:-$(nproc)}" \
    -e LIBADWAITA_SOURCE="$LibadwaitaSource" \
    "$ImageTag" bash /src/build-in-container.sh

printf ':: Packaging\n'
podman run --rm --userns=keep-id \
    -v "$StackDir:/src:ro" \
    -v "$DistDir/prefix:/opt/tuxblox-ui:ro" \
    -v "$DistDir/out:/out" \
    -e LIBADWAITA_SOURCE="$LibadwaitaSource" \
    -e LIBADWAITA_COMMIT="$LibadwaitaCommit" \
    "$ImageTag" bash /src/package.sh

rm -rf "$DistDir/dev"
mv "$DistDir/out/dev" "$DistDir/dev"
rm -f "$DistDir"/ui-stack-*.tar.zst
mv "$DistDir/out"/ui-stack-*.tar.zst "$DistDir/"
rm -rf "$DistDir/out"

printf ':: Done. Output in %s\n' "$DistDir"
