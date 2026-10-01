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

# compat/webkitgtk/bundle/verify-useragent.sh
# Re-runnable, real end-to-end check that the Roblox Player's own user agent
# survives into an HTTP request from the WebKitGTK built into the
# webkitgtk-prefix volume -- patch 5 in compat/webkitgtk/src/README-TUXBLOX-PATCHES.md.
# Compiles useragent-verify.c against the volume's own GTK4/WebKitGTK, then loads a
# page from a throwaway local server and asserts on the agent that arrived, for the
# document and for an XMLHttpRequest and an <img> subresource. A read-back from
# WebKitSettings is not equivalent proof: the bug this guards against left the
# setting looking plausible while every request carried WebKit's default agent.
#
# Usage:
#   compat/webkitgtk/bundle/verify-useragent.sh
#
# Requires: the builder image and the webkitgtk-prefix volume to already exist.
# Run this whenever WebKitGTK is rebuilt or rebased onto a new upstream release.
set -euo pipefail

ScriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# The sniper migration left two possible tags for the same image.
Image="${TUXBLOX_WEBKIT_BUILDER_IMAGE:-tuxblox-webkitgtk-builder}"
if ! podman image exists "$Image"; then
    Image="${Image}-sniper"
fi

podman run --rm -i --userns=keep-id \
    -v webkitgtk-prefix:/opt/tuxblox-webview:ro \
    -v "$ScriptDir/useragent-verify.c:/useragent-verify.c:ro" \
    "$Image" \
    bash -s <<'INNER_SCRIPT'
set -euo pipefail
Prefix=/opt/tuxblox-webview
export PKG_CONFIG_PATH="$Prefix/lib/pkgconfig:$Prefix/lib/x86_64-linux-gnu/pkgconfig"
export LD_LIBRARY_PATH="$Prefix/lib/x86_64-linux-gnu:$Prefix/lib"
export WEBKIT_EXEC_PATH="$Prefix/libexec/webkitgtk-6.0"
export GIO_MODULE_DIR="$Prefix/lib/x86_64-linux-gnu/gio/modules"
export FONTCONFIG_PATH="$Prefix/etc/fonts"
# This bundle is X11-only and software-rendered by construction, matching how
# webview2loader-host is pinned at runtime.
export GDK_BACKEND=x11 LIBGL_ALWAYS_SOFTWARE=1
export XDG_CACHE_HOME=/tmp/verify-cache XDG_RUNTIME_DIR=/tmp/verify-run
mkdir -p "$XDG_CACHE_HOME" "$XDG_RUNTIME_DIR"

Agent='RobloxNewBrowser Roblox/WinInetRobloxApp/0.740.0.7400927 (GlobalDist; RobloxDirectDownload) GAMEPADNAVIGATION'

printf ':: Compiling useragent-verify.c against the volume'"'"'s own GTK4/WebKitGTK\n'
gcc -O2 -Wall -Werror -o /tmp/useragent-verify /useragent-verify.c \
    $(pkg-config --cflags --libs gtk4 webkitgtk-6.0)

cat > /tmp/uaserver.py <<'SERVER'
import http.server
PAGE = b"""<!doctype html><meta charset=utf-8><title>ua</title>
<script>fetch('/xhr').then(r=>r.text());</script><img src="/img">"""
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def do_GET(self):
        print("WIRE %s %s" % (self.path, self.headers.get("User-Agent")), flush=True)
        body = PAGE if self.path == "/" else b"ok"
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *a): pass
class Server(http.server.HTTPServer):
    # WebKit drops its keep-alive connections as it exits, which is not an error here.
    def handle_error(self, *a): pass
Server(("127.0.0.1", 8099), Handler).serve_forever()
SERVER

python3 /tmp/uaserver.py > /tmp/wire.txt 2>&1 &
Server=$!
trap 'kill "$Server" 2>/dev/null || true' EXIT
sleep 1

printf '\n:: Checking the validator\n'
xvfb-run -a /tmp/useragent-verify http://127.0.0.1:8099/
sleep 1
kill "$Server" 2>/dev/null || true

printf '\n:: Agent seen on the wire\n'
cat /tmp/wire.txt

Failed=0
for Path in / /xhr /img; do
    if ! grep -qF "WIRE $Path $Agent" /tmp/wire.txt; then
        printf 'FAIL   %s did not carry the Player agent\n' "$Path"
        Failed=1
    fi
done
if [ "$Failed" -ne 0 ]; then
    printf '\nuseragent-verify: the agent did not reach the wire -- see patch 5 in\n'
    printf 'compat/webkitgtk/src/README-TUXBLOX-PATCHES.md\n'
    exit 1
fi

printf '\nok     the Player agent reached the wire on all three requests\n'
INNER_SCRIPT
