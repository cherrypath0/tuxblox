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

# compat/webkitgtk/bundle/build-host.sh
# Rebuilds only webview2loader-host and installs it into build/, in seconds, against
# the WebKitGTK bundle already built in the webkitgtk-prefix volume. build.sh rebuilds
# the whole dependency chain from source -- hours, and it re-downloads every tarball --
# so it is the wrong tool for a change to the host's own C.
#
# --package also repacks the shipped tarball and refreshes prebuilt/, which is what a
# release needs; it costs a few minutes of xz, so it is not the default.
set -eo pipefail
cd "$(dirname "$0")"

repo_root=$(cd ../../.. && pwd)
installed="$repo_root/build/compat/files/lib/tuxblox-webview/libexec/webview2loader-host"
do_package=0
[ "$1" = "--package" ] && do_package=1

# The image build is broken upstream of this repo -- Debian 11 is archived, so every
# pinned .deb 404s -- which is exactly why this script never builds an image and uses
# whichever one is already here instead.
image=""
for candidate in tuxblox-webkitgtk-builder tuxblox-webkitgtk-builder-sniper; do
    if podman image exists "$candidate"; then image="$candidate"; break; fi
done
if [ -z "$image" ]; then
    echo "ERROR: no builder image on this machine, and build.sh cannot make one today." >&2
    echo "       Restore one with podman load, or repair Containerfile's apt sources." >&2
    exit 1
fi

echo ":: Building webview2loader-host in $image"
podman run --rm -v "$(pwd):/src:ro" -v webkitgtk-prefix:/opt/tuxblox-webview \
    -v "$(pwd)/../../wine/dlls/webview2loader/webview2loader_ipc_protocol.h:/src/host/webview2loader_ipc_protocol.h:ro" \
    "$image" bash -c \
    "mkdir -p /build-scripts && cp /src/*.sh /src/versions.env /build-scripts/ && \
     bash /build-scripts/host-build.sh"

if [ "$do_package" = 1 ]; then
    echo ":: Packaging"
    mkdir -p out
    podman run --rm -v webkitgtk-prefix:/opt/tuxblox-webview -v "$(pwd)/out:/out" \
        -v "$(pwd)/package.sh:/build-scripts/package.sh:ro" \
        -v "$(pwd)/versions.env:/build-scripts/versions.env:ro" \
        "$image" bash /build-scripts/package.sh
    source versions.env
    cp "out/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz" ../prebuilt/
    echo ":: prebuilt/webkitgtk-${WEBKITGTK_VERSION}-x86_64.tar.xz refreshed -- commit it"
fi

if [ ! -d "$(dirname "$installed")" ]; then
    echo ":: No build/ to install into -- the binary is in the webkitgtk-prefix volume"
    exit 0
fi

# package.sh is what decides how a shipped binary is finished; these two steps are the
# part of it that one file needs, so an installed-by-hand binary is the same shape as a
# packaged one. Without the rpath it cannot find the bundle's own GTK at all.
echo ":: Installing into build/"
podman run --rm -v webkitgtk-prefix:/opt/tuxblox-webview "$image" bash -c '
    set -e
    cp -a /opt/tuxblox-webview/bin/webview2loader-host /tmp/host
    strip --strip-unneeded /tmp/host
    patchelf --force-rpath --set-rpath "\$ORIGIN:\$ORIGIN/../lib:\$ORIGIN/../lib/x86_64-linux-gnu" /tmp/host
    cat /tmp/host' > "$installed.new"
chmod 0755 "$installed.new"
# A temp file and a rename, never a copy onto the live file: anything that has it open
# loses its pages when the inode is truncated in place.
mv -f "$installed.new" "$installed"
echo ":: $installed"
