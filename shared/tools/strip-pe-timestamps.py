#!/usr/bin/env python3
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

"""Clear the moment each PE file was linked, so one commit built twice produces one layer.

A PE header carries a TimeDateStamp that the linker fills with the clock, which made 30 of the
layer's DLLs different in every build. Asking the linkers not to write it means threading a flag
through three build systems, two of which configure once and keep the answer; clearing the field
afterwards is the same result in one place, and nothing reads it: these files are not signed, and
Windows itself only consults the field for bound imports, which Wine's modules do not use.
"""

import argparse
import os
import struct
import sys


def clear(path):
    """True when the file was a PE whose stamp needed clearing.

    The staged layer is read-only, so the mode is lifted for the write and put back after.
    """
    mode = os.stat(path).st_mode
    writable = bool(mode & 0o200)
    if not writable:
        os.chmod(path, mode | 0o200)
    try:
        return _clear_open(path)
    finally:
        if not writable:
            os.chmod(path, mode)


def _clear_open(path):
    with open(path, "r+b") as handle:
        if handle.read(2) != b"MZ":
            return False
        handle.seek(0x3C)
        offset_bytes = handle.read(4)
        if len(offset_bytes) != 4:
            return False
        offset = struct.unpack("<I", offset_bytes)[0]
        handle.seek(offset)
        if handle.read(4) != b"PE\0\0":
            return False
        # COFF header: Machine, NumberOfSections, then TimeDateStamp.
        handle.seek(offset + 8)
        if handle.read(4) == b"\0\0\0\0":
            return False
        handle.seek(offset + 8)
        handle.write(b"\0\0\0\0")
        return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", help="directory to walk")
    parser.add_argument("--check", action="store_true",
                        help="report files that still carry a stamp instead of clearing them")
    args = parser.parse_args()

    cleared = 0
    left = []
    for directory, _, names in os.walk(args.root):
        for name in names:
            path = os.path.join(directory, name)
            if os.path.islink(path) or not os.path.isfile(path):
                continue
            if not name.lower().endswith((".dll", ".exe", ".drv", ".sys", ".ocx", ".cpl", ".so")):
                continue
            try:
                if args.check:
                    with open(path, "rb") as handle:
                        if handle.read(2) != b"MZ":
                            continue
                        handle.seek(0x3C)
                        offset = struct.unpack("<I", handle.read(4))[0]
                        handle.seek(offset)
                        if handle.read(4) != b"PE\0\0":
                            continue
                        handle.seek(offset + 8)
                        if handle.read(4) != b"\0\0\0\0":
                            left.append(path)
                elif clear(path):
                    cleared += 1
            except (OSError, struct.error):
                continue

    if args.check:
        for path in left[:10]:
            print("still stamped: %s" % path, file=sys.stderr)
        print(":: %d PE files still carry a link time" % len(left))
        return 1 if left else 0

    print(":: Cleared the link time from %d PE files" % cleared)
    return 0


if __name__ == "__main__":
    sys.exit(main())
