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


"""Derives a release version from the component versions that make it up.

A release is MAJOR.MINOR.BUILD.PATCH; a component is MAJOR.MINOR.PATCH and is not affected by any
of this. The two therefore disagree about what the third field means, which is why nothing here
takes a field by position across both -- `component_fields` and `parse_release` each name their
own.

  MAJOR  the highest MAJOR of any component. A rise resets MINOR and PATCH, but never BUILD.
  MINOR  advances when any component took a minor-or-bigger bump. Resets PATCH.
  BUILD  a running count of builds. It only ever rises, it is never reset by anything, only an
         experimental build advances it, and a release that carries nothing but component patches
         does not advance it either -- which is what lets a build keep its number while patches
         land on top of it.
  PATCH  patches released against the BUILD above it, so it starts again at 0 whenever BUILD
         moves.
"""

# Ranked so the largest bump across the components wins; "minor-or-bigger" is one comparison against this.
_BUMPS = ("none", "patch", "minor", "major")

EXPERIMENTAL = "experimental"


def _fields(version):
    parts = str(version).split(".")
    if len(parts) not in (3, 4) or not all(p.isdigit() for p in parts):
        raise ValueError("not a version: %r" % (version,))
    return [int(p) for p in parts]


def component_fields(version):
    """(MAJOR, MINOR, PATCH) of a component version, which is always three fields."""
    fields = _fields(version)
    if len(fields) != 3:
        raise ValueError("not a component version: %r" % (version,))
    return tuple(fields)


def parse_release(release):
    """(MAJOR, MINOR, BUILD, PATCH) of a release version.

    A three-field version is the pre-v3 shape and reads as BUILD 0, PATCH 0. A four-field version
    written under the older MAJOR.MINOR.PATCH.BUILD order reads its last two fields the other way
    round, so the first release derived across that change has to be given explicitly rather than
    derived -- that is what TUXBLOX_BUILD_VERSION is for.
    """
    fields = _fields(release)
    return tuple(fields + [0] * (4 - len(fields)))


def format_release(major, minor, build, patch):
    return "%d.%d.%d.%d" % (major, minor, build, patch)


def seed_release(seed, build):
    """The repo's own floor, as a release.

    Line 1 of the root VERSION file is MAJOR.MINOR.PATCH and line 2 is the BUILD counter, because
    BUILD counts builds rather than describing what changed. A release puts BUILD third, so the two
    cannot simply be joined -- that would read the floor's PATCH as its BUILD. A seed already
    carrying four fields is one an earlier build of this tree derived, and is taken as it stands.
    """
    fields = _fields(seed)
    if len(fields) == 4:
        return format_release(*fields)
    major, minor, patch = fields
    return format_release(major, minor, int(build), patch)


def release_sort_key(version):
    """For ordering releases against each other and taking a maximum across the channels.

    This compares the fields left to right, which orders correctly whatever each one happens to
    mean, so it is deliberately the one function that does not care. Do not unpack it to name
    fields -- `parse_release` is for that.
    """
    return parse_release(version)


def largest_bump(old, new):
    """The biggest bump level any component took, from "none" through to "major"."""
    biggest = 0
    for name, version in new.items():
        if name not in old:
            # A component that did not exist before is at least a minor event for the release.
            biggest = max(biggest, _BUMPS.index("minor"))
            continue
        was = component_fields(old[name])
        now = component_fields(version)
        if was[0] != now[0]:
            return "major"
        if was[1] != now[1]:
            biggest = max(biggest, _BUMPS.index("minor"))
        elif was[2] != now[2]:
            biggest = max(biggest, _BUMPS.index("patch"))
    return _BUMPS[biggest]


def next_build(previous_build, bump, channel):
    """The BUILD field for a new release.

    BUILD counts builds, so it is not derived from anything the server publishes, and it only ever
    rises -- nothing resets it, a MAJOR rise included, because a number that counts builds is
    continuous across every other field moving.

    Two things hold it still. Only an experimental build advances it, since that is where work is
    built until it has been tested; promoting the same work to canary or stable is not another
    build of it. And a release carrying nothing but component patches does not advance it either,
    because those are counted in PATCH instead, which is what keeps a build's number while patches
    land on top of it.
    """
    if bump not in _BUMPS:
        raise ValueError("not a bump level: %r" % (bump,))
    previous_build = int(previous_build)
    if channel != EXPERIMENTAL:
        return previous_build
    if bump == "patch":
        return previous_build
    return previous_build + 1


def derive_release(previous_max, bump, major, previous_build, channel):
    """The next release version, advancing from `previous_max` by `bump` under component `major`.

    All four fields are worked out together because they are not independent: whether PATCH starts
    again depends on whether BUILD moved.
    """
    if previous_max is None:
        # Nothing published anywhere, so there is no line to advance -- start this major's.
        return format_release(major, 0, next_build(previous_build, bump, channel), 0)

    pmajor, pminor, pbuild, ppatch = parse_release(previous_max)

    # BUILD is counted locally rather than derived, so the counter can legitimately run ahead of
    # what is published -- it counts builds that were never released. It must not run BEHIND,
    # though: now that BUILD is the third field, a counter lower than the published one would drag
    # the whole release below it. A fresh clone, or a counter that got rolled back, does exactly
    # that. So the published BUILD is a floor under the local count.
    build = next_build(max(int(previous_build), pbuild), bump, channel)
    if major < pmajor:
        # Component majors only ever rise, so a lower one means the caller computed it wrong.
        raise ValueError("major %d regresses from %s" % (major, previous_max))
    if major > pmajor:
        return format_release(major, 0, build, 0)
    if bump in ("major", "minor"):
        # A major bump on a component that does not hold the maximum lands here, and still has to advance.
        return format_release(major, pminor + 1, build, 0)
    if bump == "patch":
        # BUILD stayed where it was, so this is another patch against the same build.
        return format_release(major, pminor, build, ppatch + 1)

    # Nothing changed. PATCH starts again only if BUILD moved under it, which is what keeps the
    # number rising: on a channel that does not advance BUILD, resetting PATCH as well would
    # derive a release BELOW the one already published, and the whole scheme rests on it only ever
    # going up. Holding both is the right answer there anyway -- the same work, published again.
    return format_release(major, pminor, build, 0 if build != pbuild else ppatch)
