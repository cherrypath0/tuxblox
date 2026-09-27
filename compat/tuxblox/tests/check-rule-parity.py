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

# The account-name rule exists three times: once in C++ for the compatibility
# layer and twice in C for Wine, which cannot share a source file with it
# because the two halves carry different licences. This compares the parts that
# can drift and fails when they stop matching.

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

SOURCES = {
    "compat layer (C++)": ROOT / "compat/tuxblox/src/support/account_name.cpp",
    "advapi32 (C)": ROOT / "compat/wine/dlls/advapi32/advapi.c",
    "ntdll (C)": ROOT / "compat/wine/dlls/ntdll/unix/file.c",
}


def reservedNames(text):
    block = re.search(r"reserved(?:Names)?\s*(?:\[\])?\s*=\s*\{(.*?)\};", text,
                      re.S | re.I)
    if not block:
        return None
    return sorted(set(re.findall(r'L?"([A-Za-z0-9]+)"', block.group(1))))


def maxLength(text):
    match = re.search(r"(?:#define\s+MAX_ACCOUNT_NAME|MaxAccountNameLength\s*=)\s*(\d+)", text)
    return int(match.group(1)) if match else None


def forbiddenChars(text):
    match = re.search(r'(?:wcschr|strchr|std::string)\(\s*L?"((?:[^"\\]|\\.)*)"', text)
    if not match:
        return None
    return match.group(1)


def main():
    found = {}
    for label, path in SOURCES.items():
        if not path.is_file():
            print(f"missing source: {path}")
            return 1
        text = path.read_text()
        found[label] = (reservedNames(text), maxLength(text), forbiddenChars(text))

    failed = False
    for index, part in enumerate(["reserved names", "length limit", "forbidden characters"]):
        values = {label: value[index] for label, value in found.items()}
        if None in values.values():
            print(f"could not read the {part} from: " +
                  ", ".join(l for l, v in values.items() if v is None))
            failed = True
            continue
        if len(set(map(repr, values.values()))) != 1:
            print(f"the {part} differ:")
            for label, value in values.items():
                print(f"  {label}: {value}")
            failed = True

    if failed:
        return 1
    names, limit, chars = next(iter(found.values()))
    print(f"rule parity: all three copies agree ({len(names)} reserved names, "
          f"limit {limit}, forbidden {chars!r})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
