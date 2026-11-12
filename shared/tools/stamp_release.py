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

"""Writes the release version into the field every TuxBlox binary reserves for it.

The release is derived when a build is staged, which is the first moment it is known, so it cannot
be compiled in. Every TuxBlox program carries a fixed-size field holding the marker below and
nothing else; this finds it and writes the number after it. See shared/release/src/release_stamp.h.
"""

import sys

# Spelled out in exactly one other place, release_stamp.cpp. Both have to agree, and a binary where
# it appears twice is one where the compiler kept a second copy -- stamping one of them would leave
# the program reading the other.
MARKER = b"TUXBLOX_RELEASE_STAMP_V1:"
FIELD_SIZE = 64

# The marker, a release and a terminator have to fit the field.
MAX_RELEASE = FIELD_SIZE - len(MARKER) - 1


def stamp(path, release):
    """Writes `release` into `path` in place. Raises RuntimeError naming the file and what is wrong."""
    encoded = release.encode("utf-8")
    if not encoded:
        raise RuntimeError("%s: refusing to stamp an empty release" % path)
    if len(encoded) > MAX_RELEASE:
        raise RuntimeError("%s: release \"%s\" is %d bytes and the field holds %d"
                           % (path, release, len(encoded), MAX_RELEASE))

    # In place, through the same handle: a rewritten file would lose the executable bit and, for the
    # layer, break the hard links the build leaves beside it.
    with open(path, "r+b") as binary:
        data = binary.read()
        at = data.find(MARKER)
        if at < 0:
            raise RuntimeError("%s carries no release stamp field, so it cannot say which release "
                               "it belongs to -- it needs release_stamp.cpp compiled into it" % path)
        if data.find(MARKER, at + 1) >= 0:
            raise RuntimeError("%s carries the release stamp field more than once, so stamping one "
                               "of them would leave the program reading another" % path)
        binary.seek(at + len(MARKER))
        # The whole remainder, not just the number: re-stamping a shorter release over a longer one
        # would otherwise leave the tail of the old number behind it.
        binary.write(encoded + b"\0" * (FIELD_SIZE - len(MARKER) - len(encoded)))


def read(path):
    """The release `path` is stamped with, or "" for a binary that carries the field unwritten."""
    with open(path, "rb") as binary:
        data = binary.read()
    at = data.find(MARKER)
    if at < 0:
        return None
    field = data[at + len(MARKER):at + FIELD_SIZE]
    return field.split(b"\0", 1)[0].decode("utf-8", "replace")


def main(argv):
    if len(argv) == 2:
        found = read(argv[1])
        # A binary with no field at all and one carrying it unwritten are different problems, and
        # printing an empty line for both is how you mistake the first for the second.
        if found is None:
            print("%s carries no release stamp field" % argv[1], file=sys.stderr)
            return 1
        print(found or "(unstamped)")
        return 0
    if len(argv) != 3:
        print("usage: stamp_release.py <binary> [<release>]", file=sys.stderr)
        return 2
    stamp(argv[1], argv[2])
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except (OSError, RuntimeError) as error:
        print("!! %s" % error, file=sys.stderr)
        sys.exit(1)
