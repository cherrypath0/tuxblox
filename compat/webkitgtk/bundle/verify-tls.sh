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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
URL="${1:-https://example.com}"

podman run --rm -i \
    -v webkitgtk-prefix:/opt/tuxblox-webview \
    -v "$SCRIPT_DIR/tls-verify.c:/tls-verify.c:ro" \
    -e VERIFY_URL="$URL" \
    tuxblox-webkitgtk-builder \
    bash -s <<'INNER_SCRIPT'
set -euo pipefail
PREFIX=/opt/tuxblox-webview
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib/x86_64-linux-gnu/pkgconfig"

echo ":: Compiling tls-verify.c against the volume's real libsoup-3.0/gio-2.0"
gcc -o /tmp/tls-verify /tls-verify.c \
    $(pkg-config --cflags --libs libsoup-3.0 gio-2.0) \
    -Wl,-rpath,"$PREFIX/lib" -Wl,-rpath,"$PREFIX/lib/x86_64-linux-gnu"

echo ":: GIO module cache (confirms glib-networking's TLS module is registered)"
ls -la "$PREFIX/lib/x86_64-linux-gnu/gio/modules/"
cat "$PREFIX/lib/x86_64-linux-gnu/gio/modules/giomodule.cache" 2>/dev/null || true

echo
echo ":: Running tls-verify against $VERIFY_URL"
/tmp/tls-verify "$VERIFY_URL"
INNER_SCRIPT
