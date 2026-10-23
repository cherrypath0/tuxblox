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

from release_version import derive_release, largest_bump, release_sort_key

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


def resolve(versions, channel, fetch, seed):
    """{release, published, base_commit}: the next release version, the component versions whose bytes are already published, and the commit the previous release was built from. Raises Unreachable rather than guessing, and NotFound when a channel's pointer names a release file that is not there."""
    # The release number is shared across channels, so the maximum is taken over all of them.
    latest = {ch: _latest_of(ch, fetch) for ch in CHANNELS}
    published_releases = [r for r in latest.values() if r]

    if not published_releases:
        # Nothing published anywhere: the first v3 release is seeded from the floor, not derived,
        # because its component versions are being assigned rather than bumped.
        major = max(release_sort_key(v)[0] for v in versions.values())
        return {"release": derive_release(seed, "minor", major), "published": {}, "base_commit": None}

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
    return {"release": derive_release(maximum, largest_bump(old_versions, versions), major),
            "published": published,
            "base_commit": body.get("source_commit")}
