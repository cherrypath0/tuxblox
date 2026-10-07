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

# compat/webkitgtk/bundle/host/test_message_only_window.sh
# Standalone -- does not involve Wine. Drives the real built helper over a socketpair,
# the same way test_ipc_skeleton.sh does, and checks which webviews end up on screen.
#
# A program asks for a hidden webview by parenting its controller to HWND_MESSAGE; it owns
# a real page, used for cookies and sign-in, that must never be a window anyone can see.
# Showing one puts an empty box in the middle of the screen with a blank button beside the
# program's own in the taskbar, which is what this guards against. The other half matters
# just as much: a normal panel must still appear once its first page starts loading.
set -euo pipefail
HELPER="${1:?usage: test_message_only_window.sh /path/to/webview2loader-host}"
if [ -z "${DISPLAY:-}" ]; then
    printf 'test_message_only_window.sh: SKIP (no DISPLAY -- the helper needs an X server)\n'
    exit 0
fi
python3 - "$HELPER" <<'PYEOF'
import socket, os, struct, subprocess, sys

OP_INIT, OP_CREATE_WEBVIEW, OP_NAVIGATE_AND_WAIT, OP_GET_WINDOW_VISIBLE = 0, 1, 3, 7

helper = os.path.abspath(sys.argv[1])
bundle = os.path.dirname(os.path.dirname(helper))
a, b = socket.socketpair()
env = dict(os.environ)
# What unixlib.c points the helper at before exec'ing it; without these WebKit cannot start
# its own child processes and the helper dies on the first navigation.
env["WEBVIEW2LOADER_IPC_FD"] = str(b.fileno())
env["WEBKIT_EXEC_PATH"] = bundle + "/libexec/webkitgtk-6.0"
env["WEBKIT_INJECTED_BUNDLE_PATH"] = bundle + "/lib/webkitgtk-6.0/injected-bundle"
env["GIO_EXTRA_MODULES"] = bundle + "/lib/x86_64-linux-gnu/gio/modules"
env["GDK_BACKEND"] = "x11"
env["EGL_PLATFORM"] = "x11"
env["LIBGL_ALWAYS_SOFTWARE"] = "1"
os.set_inheritable(b.fileno(), True)
p = subprocess.Popen([helper], env=env, pass_fds=[b.fileno()], stderr=subprocess.DEVNULL)
b.close()

def call(opcode, payload, response_size):
    a.sendall(struct.pack("<I", opcode) + payload)
    buf = b""
    while len(buf) < response_size:
        chunk = a.recv(response_size - len(buf))
        assert chunk, "the helper closed the socket -- run it by hand to see its errors"
        buf += chunk
    return buf

def create(message_only):
    reply = call(OP_CREATE_WEBVIEW, struct.pack("<i4xQ", message_only, 0), 16)
    handle = struct.unpack("<i4xQ", reply)[1]
    assert handle, "the helper failed to create a webview"
    return handle

def navigate(handle, uri):
    encoded = (uri.encode("utf-16-le") + b"\0\0").ljust(4096, b"\0")
    call(OP_NAVIGATE_AND_WAIT, struct.pack("<Q", handle) + encoded + struct.pack("<i4xQ", 0, 0), 4120)

def visible(handle):
    return struct.unpack("<i", call(OP_GET_WINDOW_VISIBLE, struct.pack("<Qi4x", handle, 0), 16)[8:12])[0]

success, = struct.unpack("<i", call(OP_INIT, struct.pack("<i", 0), 4))
assert success == 1, f"expected success=1 from WV2L_OP_INIT, got {success}"

hidden = create(1)
panel = create(0)
assert not visible(hidden), "a hidden webview was on screen the moment it was created"
assert not visible(panel), "a panel was on screen before anything loaded in it"
print("neither webview is on screen before it loads anything OK")

navigate(hidden, "about:blank")
assert not visible(hidden), "a hidden webview came on screen once a page loaded in it"
print("the hidden webview stays off screen after loading a page OK")

navigate(panel, "about:blank")
assert visible(panel), "a panel never came on screen even though a page loaded in it"
print("the panel comes on screen after loading a page OK")

a.close()
p.wait(timeout=10)
PYEOF
printf 'test_message_only_window.sh: PASS\n'
