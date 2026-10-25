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

import json
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from release_resolve import NotFound, Unreachable, resolve

SEEDS = {"launcher": "3.5.0", "compat": "3.3.0", "installer": "3.1.0",
         "libtuxblox": "3.1.0", "studio-mcp": "2.0.1"}


def release_file(release, channel, versions):
    return json.dumps({
        "release": release, "channel": channel,
        "source_commit": "base1234", "source_dirty": False,
        "components": {c: {"version": v,
                           "arch": {"x86_64": {"sha256": c[0] * 64, "size": 10}, "arm64": None}}
                       for c, v in versions.items()},
    })


def fetcher(pages):
    def fetch(path):
        if path not in pages:
            raise NotFound(path)
        return pages[path]
    return fetch


class NothingPublished(unittest.TestCase):
    # The first v3 release: every channel 404s, so there is nothing to compare the components
    # against and the line cannot be said to have moved. Only BUILD advances.
    def test_all_channels_absent_keeps_the_line_and_counts_the_build(self):
        got = resolve(SEEDS, "stable", fetcher({}), seed="3.5.0", build=0)
        self.assertEqual(got["published"], {})
        self.assertEqual(got["release"], "3.5.0.1")
        # Nothing published, so there is no earlier commit to measure changes from.
        self.assertIsNone(got["base_commit"])

    # A 404 is "nothing published"; a connection failure is not something to guess past.
    def test_an_unreachable_host_refuses(self):
        def fetch(path):
            raise Unreachable(path)
        with self.assertRaises(Unreachable):
            resolve(SEEDS, "stable", fetch, seed="3.5.0", build=0)


class BuildsBetweenPublishes(unittest.TestCase):
    """BUILD counts builds, so a run of them between two publishes does not collapse onto one number."""

    def test_a_run_of_local_builds_each_get_their_own_number(self):
        pages = {"client/releases/stable/latest": "3.6.0.0\n",
                 "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS)}
        # Each run reads back the two lines the run before it wrote.
        seen, line, build = [], "3.6.0", 0
        for _ in range(3):
            release = resolve(SEEDS, "stable", fetcher(pages), seed=line, build=build)["release"]
            line, build = release.rsplit(".", 1)[0], int(release.rsplit(".", 1)[1])
            seen.append(release)
        self.assertEqual(seen, ["3.6.0.1", "3.6.0.2", "3.6.0.3"])

    def test_a_run_of_local_builds_on_a_tree_that_has_published_nothing(self):
        seen, line, build = [], "3.5.0", 0
        for _ in range(3):
            release = resolve(SEEDS, "stable", fetcher({}), seed=line, build=build)["release"]
            line, build = release.rsplit(".", 1)[0], int(release.rsplit(".", 1)[1])
            seen.append(release)
        self.assertEqual(seen, ["3.5.0.1", "3.5.0.2", "3.5.0.3"])

    # BUILD counts builds rather than describing what changed, so a component moving carries it
    # along instead of sending it back to zero. Only a MAJOR rise starts it over.
    def test_a_component_moving_carries_the_build_count_with_it(self):
        versions = dict(SEEDS, launcher="3.6.0")
        pages = {"client/releases/stable/latest": "3.6.0.0\n",
                 "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS)}
        first = resolve(versions, "stable", fetcher(pages), seed="3.6.0", build=4)["release"]
        self.assertEqual(first, "3.7.0.5")
        self.assertEqual(
            resolve(versions, "stable", fetcher(pages), seed="3.7.0", build=5)["release"], "3.7.0.6")

    def test_a_major_rise_starts_the_build_count_over(self):
        versions = dict(SEEDS, launcher="4.0.0")
        pages = {"client/releases/stable/latest": "3.6.0.0\n",
                 "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS)}
        self.assertEqual(
            resolve(versions, "stable", fetcher(pages), seed="3.6.0", build=31)["release"], "4.0.0.0")

    # The reuse map and the base commit describe the PUBLISHED release, and the seed is local, so it
    # must not reach either of them.
    def test_the_seed_does_not_touch_what_is_already_published(self):
        pages = {"client/releases/stable/latest": "3.6.0.0\n",
                 "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS)}
        got = resolve(SEEDS, "stable", fetcher(pages), seed="3.6.0", build=9)
        self.assertEqual(got["base_commit"], "base1234")
        self.assertEqual(got["published"][("launcher", "3.5.0")]["size"], 10)


class Derives(unittest.TestCase):
    def test_takes_the_maximum_across_channels_not_this_channel_only(self):
        pages = {
            "client/releases/stable/latest": "3.6.0.0\n",
            "client/releases/canary/latest": "3.7.0.2\n",
            "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS),
            "client/releases/canary/3.7.0.2.json": release_file("3.7.0.2", "canary", SEEDS),
        }
        got = resolve(SEEDS, "stable", fetcher(pages), seed="3.5.0", build=0)
        # Nothing changed against the maximum, so the line is a rebuild of it -- canary's, not this
        # channel's. BUILD is this tree's own count and owes nothing to the number canary reached.
        self.assertEqual(got["release"], "3.7.0.1")

    def test_a_patch_against_the_maximum_advances_patch(self):
        versions = dict(SEEDS, launcher="3.5.1")
        pages = {"client/releases/stable/latest": "3.6.0.0\n",
                 "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS)}
        got = resolve(versions, "stable", fetcher(pages), seed="3.5.0", build=0)
        # PATCH advances off the published maximum; BUILD keeps counting rather than resetting.
        self.assertEqual(got["release"], "3.6.1.1")
        # The commit the maximum release was built from: what "changed since" is measured against.
        self.assertEqual(got["base_commit"], "base1234")

    def test_published_map_comes_from_the_maximum_release(self):
        pages = {"client/releases/stable/latest": "3.6.0.0\n",
                 "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS)}
        published = resolve(SEEDS, "stable", fetcher(pages), seed="3.5.0", build=0)["published"]
        self.assertEqual(published[("compat", "3.3.0")], {"sha256": "c" * 64, "size": 10})
        self.assertEqual(set(published), {(c, v) for c, v in SEEDS.items()})

    # An architecture that was never published has no bytes to reuse.
    def test_a_null_arch_contributes_nothing_to_the_published_map(self):
        pages = {"client/releases/stable/latest": "3.6.0.0\n",
                 "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS)}
        published = resolve(SEEDS, "stable", fetcher(pages), seed="3.5.0", build=0)["published"]
        self.assertTrue(all("sha256" in e for e in published.values()))

    # The pointer exists but the file it names does not: that is a broken channel, not an empty one.
    def test_a_latest_naming_a_missing_release_file_raises(self):
        pages = {"client/releases/stable/latest": "3.6.0.0\n"}
        with self.assertRaises(NotFound):
            resolve(SEEDS, "stable", fetcher(pages), seed="3.5.0", build=0)


if __name__ == "__main__":
    unittest.main()
