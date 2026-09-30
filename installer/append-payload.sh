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

if [[ $# -ne 4 ]]; then
    printf 'usage: %s <installer> <ui-stack.tar.zst> <interface-binary> <output>\n' "$0" >&2
    exit 2
fi

Outer="$1"
StackTar="$2"
UiBin="$3"
Out="$4"

Staging="$(mktemp -d)"
trap 'rm -rf "$Staging"' EXIT

tar -xf "$StackTar" -C "$Staging"
install -m 0755 "$UiBin" "$Staging/TuxBloxInstaller-ui"

for required in TuxBloxInstaller-ui lib fonts/fonts.conf share/glib-2.0/schemas; do
    if [[ ! -e "$Staging/$required" ]]; then
        printf '!! Payload is missing %s\n' "$required" >&2
        exit 1
    fi
done

Payload="$Staging.tar.zst"
Combined="$Out.partial"
trap 'rm -rf "$Staging" "$Payload" "$Combined"' EXIT

tar --sort=name --mtime="@${SOURCE_DATE_EPOCH:-0}" --owner=0 --group=0 --numeric-owner -C "$Staging" -cf - . | zstd -19 -T0 -q --force -o "$Payload"

# Writes the outer binary without any payload already on it, then the payload and its trailer. Prints nothing.
python3 - "$Outer" "$Payload" "$Combined" <<'PY'
import hashlib, struct, sys

outerPath, payloadPath, outPath = sys.argv[1:4]
magic = b"TUXBLOXUI\0"
trailerSize = 62

outer = open(outerPath, "rb").read()
if outer[-trailerSize:-trailerSize + len(magic)] == magic:
    fmt, = struct.unpack("<I", outer[-trailerSize + 10:-trailerSize + 14])
    off, size = struct.unpack("<QQ", outer[-trailerSize + 14:-trailerSize + 30])
    digest = outer[-32:]
    if fmt != 1 or off + size != len(outer) - trailerSize or hashlib.sha256(outer[off:off + size]).digest() != digest:
        sys.exit("!! " + outerPath + " carries an interface trailer but is not a valid packed installer, refusing to guess")
    outer = outer[:off]

payload = open(payloadPath, "rb").read()
trailer = magic + struct.pack("<I", 1) + struct.pack("<Q", len(outer)) + struct.pack("<Q", len(payload)) + hashlib.sha256(payload).digest()
assert len(trailer) == trailerSize, len(trailer)

with open(outPath, "wb") as f:
    f.write(outer)
    f.write(payload)
    f.write(trailer)
PY

chmod 0755 "$Combined"
mv -f "$Combined" "$Out"
printf ':: %s -- %s bytes\n' "$Out" "$(stat -c %s "$Out")"
