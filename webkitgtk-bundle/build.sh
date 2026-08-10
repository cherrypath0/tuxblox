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
podman run --rm -v "$(pwd):/src:ro" -v webkitgtk-prefix:/opt/tuxblox-webview \
    -v webkitgtk-ccache:/ccache \
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
