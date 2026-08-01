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


class Record(NamedTuple):
    seq: int
    tid: str
    surface: str
    rip: int
    detail: str


# Wine prefixes trace output with "trace:tuxblox:<function> ", so the marker
# is matched anywhere in the line rather than anchored at column 0.
_REC_RE = re.compile(
    r"\bREC\s+seq=(\d+)\s+tid=(\S+)\s+surface=(\S+)\s+rip=(0x[0-9a-fA-F]+)\s+detail=(.*)$"
)


def parse_records(lines: Iterable[str]) -> List[Record]:
    """Extract tracer records from a raw trace log."""
    records = []
    for line in lines:
        m = _REC_RE.search(line.rstrip("\n"))
        if not m:
            continue
        seq, tid, surface, rip, detail = m.groups()
        records.append(Record(
            seq=int(seq),
            tid=tid,
            surface=surface,
            rip=int(rip, 16),
            detail=detail,
        ))
    return records


def build_report(records: List[Record], ranges: List[MapRange]) -> str:
    """Render records grouped by calling module, ordered by first touch.

    Modules and surfaces are both ordered by the sequence number of their
    first appearance rather than by volume: for this investigation, what
    Hyperion touched *first* is more informative than what it touched most,
    since the process dies early.
    """
    by_module = {}
    for rec in records:
        hit = resolve(rec.rip, ranges)
        module = hit[0] if hit else "<unresolved>"
        rva = hit[1] if hit else rec.rip
        surfaces = by_module.setdefault(module, {})
        entry = surfaces.setdefault(
            rec.surface, {"count": 0, "first_seq": rec.seq, "rvas": set(),
                          "details": []})
        entry["count"] += 1
        entry["rvas"].add(rva)
        if len(entry["details"]) < 5 and rec.detail:
            entry["details"].append(rec.detail)

    def module_first_seq(item):
        return min(e["first_seq"] for e in item[1].values())

    out = ["TuxBlox fingerprint trace report",
           "=" * 60,
           f"records: {len(records)}   modules: {len(by_module)}",
           ""]

    for module, surfaces in sorted(by_module.items(), key=module_first_seq):
        out.append(f"caller module: {module}")
        for surface, e in sorted(surfaces.items(), key=lambda i: i[1]["first_seq"]):
            rvas = ", ".join(f"+{r:#x}" for r in sorted(e["rvas"])[:4])
            out.append(f"  {surface:<32} calls={e['count']:<6} "
                       f"first_seq={e['first_seq']:<6} at {rvas}")
            for d in e["details"]:
                out.append(f"      detail: {d}")
        out.append("")

    return "\n".join(out)


def main(argv: List[str]) -> int:
    import argparse

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("trace", help="trace log captured from a WINEDEBUG=+tuxblox run")
    args = ap.parse_args(argv)

    with open(args.trace, "r", errors="replace") as fh:
        lines = fh.readlines()

    ranges = parse_maps(lines)
    if not ranges:
        print("error: no /proc/self/maps lines found in trace -- "
              "the maps dump did not run, so addresses cannot be resolved")
        return 1

    print(build_report(parse_records(lines), ranges))
    return 0


if __name__ == "__main__":
    import sys as _sys
    raise SystemExit(main(_sys.argv[1:]))
