#!/bin/bash
# webkitgtk-bundle/build.sh
# One-time (or per-version-bump) build. NOT part of the main repo's build.sh -- run
# manually by a maintainer when WebKitGTK needs building or updating. See README.md.
set -eo pipefail
cd "$(dirname "$0")"

echo ":: Building builder container image"
podman build -t tuxblox-webkitgtk-builder -f Containerfile .

echo ":: Running dependency + webkitgtk build (this takes a long time -- hours on a
::    from-scratch run; much faster on a re-run with a warm webkitgtk-ccache volume,
::    since only the WebKitGTK step itself is cached, see build-in-container.sh)"
# Task 8 post-review correction (2026-08-10), I-1: -e JOBS=1 is not a conservative
# default here, it's the only setting build-in-container.sh's own WebKitGTK cmake
# step comment documents as actually validated end-to-end -- JOBS=2 was confirmed to
# OOM-kill partway through a full build (inside WebCore, not just the JSC subset that
# misleadingly looked safe on its own; see that comment for the full JOBS/memory
# history). Leaving this unset would silently default to $(nproc) via
# build-in-container.sh's own `JOBS="${JOBS:-$(nproc)}"` fallback -- fine for the
# earlier meson/autotools dependencies, which this JOBS history has no data on either
# way, but a real, reproduced OOM risk specifically once the script reaches
# WebKitGTK's own build. A caller who has verified higher parallelism is safe on
# their own machine can still override this by setting JOBS in their own environment
# before invoking this script (`env -u` isn't used here, so an inherited JOBS from
# the caller's shell would silently be ignored by `-e JOBS=1` -- if that ever needs
# to be caller-overridable, change this to `-e JOBS="${JOBS:-1}"` instead).
podman run --rm -v "$(pwd):/src:ro" -v webkitgtk-prefix:/opt/tuxblox-webview \
    -v webkitgtk-ccache:/ccache -e JOBS=1 \
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
