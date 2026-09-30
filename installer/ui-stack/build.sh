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
RepoRoot="$(cd ../.. && pwd)"
Artifacts="$RepoRoot/build/.artifacts/ui-stack"
ImageTag=tuxblox-ui-stack-builder

mkdir -p "$Artifacts/prefix" "$Artifacts/work" "$Artifacts/out"

printf ':: Building the builder image\n'
podman build -t "$ImageTag" -f Containerfile .

printf ':: Building the stack (a rerun skips every library that already finished)\n'
podman run --rm --userns=keep-id \
    -v "$StackDir:/src:ro" \
    -v "$Artifacts/prefix:/opt/tuxblox-ui" \
    -v "$Artifacts/work:/build" \
    -e JOBS="${JOBS:-$(nproc)}" \
    "$ImageTag" bash /src/build-in-container.sh

printf ':: Packaging\n'
podman run --rm --userns=keep-id \
    -v "$StackDir:/src:ro" \
    -v "$Artifacts/prefix:/opt/tuxblox-ui:ro" \
    -v "$Artifacts/out:/out" \
    "$ImageTag" bash /src/package.sh

rm -rf "$Artifacts/dev"
mv "$Artifacts/out/dev" "$Artifacts/dev"
mv "$Artifacts/out"/ui-stack-*.tar.zst "$Artifacts/"
rm -rf "$Artifacts/out"

printf ':: Done. Output in %s\n' "$Artifacts"
