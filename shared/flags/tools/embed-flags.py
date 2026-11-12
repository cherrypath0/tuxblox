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

"""Builds a flag file into a C++ header.

The defaults are compiled into every binary, so there is no runtime to catch a bad one:
a name that declares no type, or a value that contradicts the one it declares, fails the
build here instead. The same checks run again at runtime against the server's answer and
the user's override, because those arrive from outside the build.

    embed-flags.py <input.json> <output.h> <symbol>
    embed-flags.py <input.json> --publish-json <output.json>

One file holds every flag, and the two tiers of the ladder are made from it: the header
is compiled in as what a build falls back to, and the JSON is what is published to the
server. The second drops the flags a client will only honour locally, because publishing
one means setting it on the server and watching nothing happen.
"""

import json
import sys

PREFIXES = (("tbxFlag", bool), ("tbxString", str), ("tbxInt", int))
# Mirrors kLocalOnly in shared/flags/FlagManager.cpp. The client drops these from a
# server answer anyway; dropping them here as well means the server is never told a value
# it would be ignored for, and the published set says only what it can decide.
LOCAL_ONLY = ("tbxFlagDisableUpdater",)
# A delimiter that cannot occur inside JSON, so the literal cannot be closed early.
DELIM = "TBXFLAGS"


def fail(message):
    sys.stderr.write(f"embed-flags: {message}\n")
    raise SystemExit(1)


def declared_type(name):
    for prefix, kind in PREFIXES:
        if len(name) > len(prefix) and name.startswith(prefix):
            return kind
    return None


def check(flags):
    for name, value in flags.items():
        kind = declared_type(name)
        if kind is None:
            fail(f"{name} does not start with tbxFlag, tbxString or tbxInt")
        # bool is a subclass of int in Python, so an int flag must not accept True and a
        # bool flag must not accept 1 -- the C++ side refuses both for the same reason.
        if kind is bool and not isinstance(value, bool):
            fail(f"{name} is not a boolean")
        if kind is str and not isinstance(value, str):
            fail(f"{name} is not a string")
        if kind is int and (isinstance(value, bool) or not isinstance(value, int)):
            fail(f"{name} is not an integer")
        # Past 2**53 a JSON number no longer survives the trip unchanged, and past 2**63
        # the launcher reads it as a large negative. Refused where it is written.
        if kind is int and not -(2 ** 53) < value < 2 ** 53:
            fail(f"{name} is too large to be delivered intact")


def main(argv):
    publishing = "--publish-json" in argv
    args = [a for a in argv[1:] if a != "--publish-json"]
    if len(args) != (2 if publishing else 3):
        fail("usage: embed-flags.py <input.json> <output.h> <symbol>\n"
             "   or: embed-flags.py <input.json> --publish-json <output.json>")
    source, destination = args[0], args[1]
    symbol = None if publishing else args[2]

    try:
        with open(source, encoding="utf-8") as handle:
            flags = json.load(handle)
    except OSError as err:
        fail(f"cannot read {source}: {err}")
    except json.JSONDecodeError as err:
        fail(f"{source} is not valid JSON: {err}")

    if not isinstance(flags, dict):
        fail(f"{source} must hold a flat JSON object of flags")
    check(flags)

    if publishing:
        # Said out loud rather than dropped quietly: somebody reading the deploy output
        # should see that the set on the server is not the whole file.
        kept = {}
        for name, value in flags.items():
            if name in LOCAL_ONLY:
                sys.stderr.write(f"embed-flags: not publishing {name}; it is only honoured "
                                 f"on a user's own machine\n")
                continue
            kept[name] = value
        with open(destination, "w", encoding="utf-8") as handle:
            handle.write(json.dumps(kept, sort_keys=True, indent=4) + "\n")
        return

    # Re-serialised rather than copied: what is compiled in is then exactly what passed
    # the checks above, whatever the file's formatting happened to be.
    body = json.dumps(flags, sort_keys=True)
    with open(destination, "w", encoding="utf-8") as handle:
        handle.write("// Generated by shared/flags/tools/embed-flags.py. Do not edit.\n")
        handle.write("#pragma once\n\n")
        handle.write(f'inline constexpr const char* {symbol} = R"{DELIM}({body}){DELIM}";\n')


if __name__ == "__main__":
    main(sys.argv)
