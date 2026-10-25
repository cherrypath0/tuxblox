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

"""Resolves the next release version, and which component versions are already published."""

import json

from release_version import derive_line, largest_bump, next_build, release_sort_key

CHANNELS = ("stable", "canary", "experimental")


class NotFound(Exception):
    """The path is not on the server. For a channel's `latest` that means nothing is published yet."""


class Unreachable(Exception):
    """The server could not be reached, which is not something to guess past."""


def _latest_of(channel, fetch):
    try:
        return fetch("client/releases/%s/latest" % channel).strip()
    except NotFound:
        return None


def _release(line, seed, build):
    """`line` with its BUILD field, which advances unless MAJOR moved."""
    seed_major = release_sort_key(seed)[0] if seed else None
    return "%s.%d" % (line, next_build(build, release_sort_key(line)[0] != seed_major))


def resolve(versions, channel, fetch, seed, build):
    """{release, published, base_commit}: the next release version, the component versions whose bytes are already published, and the commit the previous release was built from. Raises Unreachable rather than guessing, and NotFound when a channel's pointer names a release file that is not there."""
    # The release number is shared across channels, so the maximum is taken over all of them.
    latest = {ch: _latest_of(ch, fetch) for ch in CHANNELS}
    published_releases = [r for r in latest.values() if r]

    if not published_releases:
        # Nothing published anywhere: the first v3 release is seeded from the floor, not derived,
        # because its component versions are being assigned rather than bumped. A seed already
        # carrying four fields is one an earlier build of this tree derived and wrote back, and
        # there is still nothing published to derive from, so the next build advances its BUILD
        # rather than assigning a second first release.
        # Nothing published is nothing to compare against, so the line cannot be said to have moved
        # and only BUILD advances. A component MAJOR above the seed's still takes the line with it.
        major = max(release_sort_key(v)[0] for v in versions.values())
        return {"release": _release(derive_line(seed, "none", major), seed, build),
                "published": {}, "base_commit": None}

    maximum = max(published_releases, key=release_sort_key)
    maximum_channel = next(ch for ch, r in latest.items() if r == maximum)
    body = json.loads(fetch("client/releases/%s/%s.json" % (maximum_channel, maximum)))

    old_versions = {}
    published = {}
    for component, entry in body["components"].items():
        old_versions[component] = entry["version"]
        bytes_for_arch = entry["arch"].get("x86_64")
        if bytes_for_arch:
            published[(component, entry["version"])] = {"sha256": bytes_for_arch["sha256"],
                                                         "size": bytes_for_arch["size"]}

    major = max(release_sort_key(v)[0] for v in versions.values())
    # The floor is what the channels publish; the seed only carries BUILD, so a run of local builds
    # that publishes none of them still gets a number each rather than all sharing one.
    line = derive_line(maximum, largest_bump(old_versions, versions), major)
    return {"release": _release(line, seed, build),
            "published": published,
            "base_commit": body.get("source_commit")}
