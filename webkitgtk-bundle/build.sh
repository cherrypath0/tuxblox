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

# webkitgtk-bundle/build.sh
# One-time (or per-version-bump) build. NOT part of the main repo's build.sh -- run
# manually by a maintainer when WebKitGTK needs building or updating. See README.md.
set -eo pipefail
cd "$(dirname "$0")"

echo ":: Building builder container image"
podman build -t tuxblox-webkitgtk-builder -f Containerfile .

echo ":: Running dependency + webkitgtk build (this takes a long time -- hours on a
::    from-scratch run; much faster on a re-run since WebKitGTK's own source+build
::    tree persists in the webkitgtk-src-build volume -- ninja does a true
::    incremental build and finishes in seconds if nothing in webkitgtk itself
::    changed, see build-in-container.sh's webkitgtk section)"
# Task 8 post-review correction (2026-08-10), I-1 found JOBS=2 OOM-killing partway
# through a full build (inside WebCore, not just the JSC subset that misleadingly
# looked safe on its own) and JOBS=1 was the only setting validated end-to-end at the
# time. 2026-08-15: repo owner explicitly re-confirmed JOBS=4 twice in the same day
# against this same host, both real full builds (one after a genuine source patch to
# WebKitWebViewBase.cpp forcing a real recompile, not just a cached no-op run),
# neither hit the OOM class above -- promoted back to the real default rather than a
# one-off override. If a future run on a lower-memory host hits that OOM class again,
# drop back to JOBS=1 and update this comment with the new data point, same as I-1 did.
podman run --rm -v "$(pwd):/src:ro" -v webkitgtk-prefix:/opt/tuxblox-webview \
    -v "$(pwd)/../ProtonSource/wine/dlls/webview2loader/webview2loader_ipc_protocol.h:/src/host/webview2loader_ipc_protocol.h:ro" \
    -v "$(pwd)/../ProtonSource/webkitgtk:/src-webkitgtk:ro" \
    -v webkitgtk-ccache:/ccache -v webkitgtk-src-build:/build/webkitgtk -e JOBS="${JOBS:-4}" \
    tuxblox-webkitgtk-builder bash -c \
    "mkdir -p /build-scripts && cp /src/*.sh /src/versions.env /build-scripts/ && \
     bash /build-scripts/build-in-container.sh"

echo ":: Packaging"
mkdir -p out
podman run --rm -v webkitgtk-prefix:/opt/tuxblox-webview -v "$(pwd)/out:/out" \
    -v "$(pwd)/package.sh:/build-scripts/package.sh:ro" \
    -v "$(pwd)/versions.env:/build-scripts/versions.env:ro" \
    tuxblox-webkitgtk-builder bash /build-scripts/package.sh

source versions.env
echo ":: Done. Output: out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz"
echo ":: Next: cp out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz ../ProtonSource/contrib/ && git add/commit it"
