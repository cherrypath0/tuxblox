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

"""Derives a release version from the component versions that make it up."""

# Ranked so the largest bump across the components wins; "minor-or-bigger" is one comparison against this.
_BUMPS = ("none", "patch", "minor", "major")


def release_sort_key(release):
    """The four fields as integers, for ordering and for taking a maximum across channels."""
    parts = str(release).split(".")
    if len(parts) not in (3, 4) or not all(p.isdigit() for p in parts):
        raise ValueError("not a version: %r" % (release,))
    fields = [int(p) for p in parts]
    # A three-field version is the pre-v3 shape, and a release is ordered against it as if BUILD were 0.
    return tuple(fields + [0] * (4 - len(fields)))


def largest_bump(old, new):
    """The biggest bump level any component took, from "none" through to "major"."""
    biggest = 0
    for name, version in new.items():
        if name not in old:
            # A component that did not exist before is at least a minor event for the release.
            biggest = max(biggest, _BUMPS.index("minor"))
            continue
        was = release_sort_key(old[name])
        now = release_sort_key(version)
        if was[0] != now[0]:
            return "major"
        if was[1] != now[1]:
            biggest = max(biggest, _BUMPS.index("minor"))
        elif was[2] != now[2]:
            biggest = max(biggest, _BUMPS.index("patch"))
    return _BUMPS[biggest]


def next_build(previous_build, major_changed):
    """The BUILD field for a new release: one past the last, or 0 when MAJOR moved.

    BUILD counts how many times this has been built, so it is not derived from anything the server
    publishes -- every build advances it, including the ones that are never released. Only a MAJOR
    rise starts it over, because that is the one event the number below it is not continuous across.
    """
    if major_changed:
        return 0
    return int(previous_build) + 1


def derive_line(previous_max, bump, major):
    """The next MAJOR.MINOR.PATCH, advancing from `previous_max` by `bump` under the component `major`."""
    if bump not in _BUMPS:
        raise ValueError("not a bump level: %r" % (bump,))
    if previous_max is None:
        return "%d.0.0" % major
    pmajor, pminor, ppatch, _ = release_sort_key(previous_max)
    if major < pmajor:
        # Component majors only ever rise, so a lower one means the caller computed it wrong.
        raise ValueError("major %d regresses from %s" % (major, previous_max))
    if major > pmajor:
        return "%d.0.0" % major
    if bump in ("major", "minor"):
        # A major bump on a component that does not hold the maximum lands here, and still has to advance.
        return "%d.%d.0" % (pmajor, pminor + 1)
    if bump == "patch":
        return "%d.%d.%d" % (pmajor, pminor, ppatch + 1)
    return "%d.%d.%d" % (pmajor, pminor, ppatch)
