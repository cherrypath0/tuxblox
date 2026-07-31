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

"""Symbolize and summarize a TuxBlox fingerprint trace.

Consumes the stderr of a Player run launched with WINEDEBUG=+tuxblox and
turns the raw instruction pointers in each record into (module, RVA) pairs,
grouped by calling module.

Symbolization is deliberately offline: the tracer inside ntdll logs raw
addresses plus a one-shot /proc/self/maps dump, which avoids calling PE-side
code from unix-side code. See the design spec for the full rationale.
"""

import re
from typing import Iterable, List, NamedTuple, Optional, Tuple


class MapRange(NamedTuple):
    start: int
    end: int
    perms: str
    file_offset: int
    path: str


# address-range perms offset dev inode [pathname]
_MAPS_RE = re.compile(
    r"^([0-9a-fA-F]+)-([0-9a-fA-F]+)\s+(\S{4})\s+([0-9a-fA-F]+)\s+\S+\s+\d+\s*(.*)$"
)

# The tracer emits maps lines as "MAPS <line>", and Wine prefixes all trace
# output with "trace:<channel>:<function> ". Strip everything up to and
# including the marker before matching, while still accepting a bare
# /proc/self/maps line so this can be pointed at the kernel file directly.
_MAPS_MARKER = "MAPS "


def parse_maps(lines: Iterable[str]) -> List[MapRange]:
    """Parse /proc/self/maps lines into MapRange records.

    Lines that do not match the kernel's maps format are skipped, so this
    can be fed the whole trace file rather than a pre-filtered slice.
    """
    ranges = []
    for line in lines:
        line = line.rstrip("\n")
        marker = line.find(_MAPS_MARKER)
        if marker != -1:
            line = line[marker + len(_MAPS_MARKER):]
        m = _MAPS_RE.match(line.strip())
        if not m:
            continue
        start, end, perms, offset, path = m.groups()
        ranges.append(MapRange(
            start=int(start, 16),
            end=int(end, 16),
            perms=perms,
            file_offset=int(offset, 16),
            path=path.strip(),
        ))
    return ranges


def resolve(addr: int, ranges: List[MapRange]) -> Optional[Tuple[str, int]]:
    """Resolve an absolute address to (module_path, RVA).

    The RVA is file_offset + (addr - start). Using (addr - start) alone is
    WRONG: a DLL's r-xp segment is not necessarily mapped at its own file
    offset 0, and that mistake produced a false negative during the item 6
    stack-dump investigation.
    """
    for r in ranges:
        if r.start <= addr < r.end:
            return (r.path or "<anonymous>", r.file_offset + (addr - r.start))
    return None
