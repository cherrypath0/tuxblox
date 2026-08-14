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

# webkitgtk-bundle/host/test_ipc_skeleton.sh
# Standalone -- does not involve Wine. Forks the real built helper binary
# directly over a real socketpair via a tiny python harness (available on
# any dev machine per this repo's own stated python3 prerequisite).
set -euo pipefail
HELPER="${1:?usage: test_ipc_skeleton.sh /path/to/webview2loader-host}"
python3 - "$HELPER" <<'PYEOF'
import socket, os, struct, subprocess, sys, time
helper = sys.argv[1]
a, b = socket.socketpair()
env = dict(os.environ)
env["WEBVIEW2LOADER_IPC_FD"] = str(b.fileno())
os.set_inheritable(b.fileno(), True)
p = subprocess.Popen([helper], env=env, pass_fds=[b.fileno()])
b.close()
# WV2L_OP_INIT = 0, struct wv2l_init_params { int32_t success; } -> 4 bytes
a.sendall(struct.pack("<I", 0) + struct.pack("<i", 0))
resp = a.recv(4)
success, = struct.unpack("<i", resp)
assert success == 1, f"expected success=1, got {success}"
print("WV2L_OP_INIT round-trip OK")
a.close()
p.wait(timeout=5)
PYEOF
echo "test_ipc_skeleton.sh: PASS"
