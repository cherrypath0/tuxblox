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

PREFIX="${PREFIX:-/opt/tuxblox-webview}"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig"

echo ":: Generating webview2loader-host error page"
WV2L_GENERATED=/tmp/webview2loader-host-generated
mkdir -p "$WV2L_GENERATED"
python3 /src/host/generate-errorpage-template.py "$WV2L_GENERATED"
python3 /src/host/fonts/generate-fonts-header.py "$WV2L_GENERATED"

echo ":: Building webview2loader-host"
gcc -O2 -Wall -Werror -o "$PREFIX/bin/webview2loader-host" \
    /src/host/main.c /src/host/ipc.c /src/host/webview.c /src/host/geometry.c /src/host/navigate.c \
    /src/host/watchdog.c /src/host/errorpage.c \
    -I"$WV2L_GENERATED" -I/src/host \
    $(pkg-config --cflags gtk4 webkitgtk-6.0) \
    $(pkg-config --libs gtk4 webkitgtk-6.0) \
    -lX11 -ldl
