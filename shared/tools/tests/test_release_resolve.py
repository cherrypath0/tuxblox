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
from release_version import parse_release

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


def published_stable(release, versions=None):
    return {"client/releases/stable/latest": release + "\n",
            "client/releases/stable/%s.json" % release:
                release_file(release, "stable", versions or SEEDS)}


class NothingPublished(unittest.TestCase):
    # Every channel 404s, so there is nothing to compare the components against and the release
    # cannot be said to have moved. Only an experimental build advances BUILD.
    def test_an_experimental_build_counts(self):
        got = resolve(SEEDS, "experimental", fetcher({}), seed="3.5.0", build=0)
        self.assertEqual(got["published"], {})
        self.assertEqual(got["release"], "3.5.1.0")
        # Nothing published, so there is no earlier commit to measure changes from.
        self.assertIsNone(got["base_commit"])

    def test_another_channel_does_not_count_a_build(self):
        for channel in ("stable", "canary"):
            got = resolve(SEEDS, channel, fetcher({}), seed="3.5.0", build=0)
            self.assertEqual(got["release"], "3.5.0.0")

    # A 404 is "nothing published"; a connection failure is not something to guess past.
    def test_an_unreachable_host_refuses(self):
        def fetch(path):
            raise Unreachable(path)
        with self.assertRaises(Unreachable):
            resolve(SEEDS, "stable", fetch, seed="3.5.0", build=0)


class BuildsBetweenPublishes(unittest.TestCase):
    """BUILD counts builds, so a run of them between two publishes does not collapse onto one."""

    def test_a_run_of_experimental_builds_each_get_their_own_number(self):
        pages = published_stable("3.6.0.0")
        seed, build, seen = "3.6.0", 0, []
        for _ in range(3):
            release = resolve(SEEDS, "experimental", fetcher(pages), seed=seed, build=build)["release"]
            major, minor, build, _patch = parse_release(release)
            seed = "%d.%d.0" % (major, minor)
            seen.append(release)
        self.assertEqual(seen, ["3.6.1.0", "3.6.2.0", "3.6.3.0"])

    def test_a_run_of_builds_on_a_tree_that_has_published_nothing(self):
        seed, build, seen = "3.5.0", 0, []
        for _ in range(3):
            release = resolve(SEEDS, "experimental", fetcher({}), seed=seed, build=build)["release"]
            major, minor, build, _patch = parse_release(release)
            seed = "%d.%d.0" % (major, minor)
            seen.append(release)
        self.assertEqual(seen, ["3.5.1.0", "3.5.2.0", "3.5.3.0"])

    # BUILD counts builds rather than describing what changed, so nothing sends it back to zero.
    def test_a_component_moving_carries_the_build_count_with_it(self):
        versions = dict(SEEDS, launcher="3.6.0")
        pages = published_stable("3.6.4.0")
        self.assertEqual(
            resolve(versions, "experimental", fetcher(pages), seed="3.6.0", build=4)["release"],
            "3.7.5.0")

    # This is the whole difference from the scheme before it.
    def test_a_major_rise_does_not_start_the_build_count_over(self):
        versions = dict(SEEDS, launcher="4.0.0")
        pages = published_stable("3.6.31.0")
        self.assertEqual(
            resolve(versions, "experimental", fetcher(pages), seed="3.6.0", build=31)["release"],
            "4.0.32.0")

    # The reuse map and the base commit describe the PUBLISHED release, and the seed is local, so it
    # must not reach either of them.
    def test_the_seed_does_not_touch_what_is_already_published(self):
        pages = published_stable("3.6.9.0")
        got = resolve(SEEDS, "stable", fetcher(pages), seed="3.6.0", build=9)
        self.assertEqual(got["base_commit"], "base1234")
        self.assertEqual(got["published"][("launcher", "3.5.0")]["size"], 10)


class PatchesLandUnderTheirBuild(unittest.TestCase):
    """What the field order exists for: a patch release keeps the BUILD it applies to."""

    def test_a_patch_against_the_maximum_holds_build_and_advances_patch(self):
        versions = dict(SEEDS, launcher="3.5.1")
        pages = published_stable("3.6.7.0")
        got = resolve(versions, "experimental", fetcher(pages), seed="3.6.0", build=7)
        self.assertEqual(got["release"], "3.6.7.1")
        self.assertEqual(got["base_commit"], "base1234")

    def test_a_second_patch_advances_patch_again(self):
        versions = dict(SEEDS, launcher="3.5.2")
        pages = published_stable("3.6.7.1", dict(SEEDS, launcher="3.5.1"))
        self.assertEqual(
            resolve(versions, "experimental", fetcher(pages), seed="3.6.0", build=7)["release"],
            "3.6.7.2")

    def test_a_minor_after_patches_advances_build_and_clears_patch(self):
        versions = dict(SEEDS, launcher="3.6.0")
        pages = published_stable("3.6.7.2", dict(SEEDS, launcher="3.5.2"))
        self.assertEqual(
            resolve(versions, "experimental", fetcher(pages), seed="3.6.0", build=7)["release"],
            "3.7.8.0")


class Derives(unittest.TestCase):
    def test_takes_the_maximum_across_channels_not_this_channel_only(self):
        pages = {
            "client/releases/stable/latest": "3.6.0.0\n",
            "client/releases/canary/latest": "3.7.2.0\n",
            "client/releases/stable/3.6.0.0.json": release_file("3.6.0.0", "stable", SEEDS),
            "client/releases/canary/3.7.2.0.json": release_file("3.7.2.0", "canary", SEEDS),
        }
        got = resolve(SEEDS, "experimental", fetcher(pages), seed="3.5.0", build=0)
        # Nothing changed against the maximum, so this is a rebuild of canary's release, not this
        # channel's. BUILD is floored at the maximum's so the number cannot go backwards.
        self.assertEqual(got["release"], "3.7.3.0")

    # The published BUILD floors the local counter: a fresh clone counts from zero and must not
    # derive a release below what is already out there.
    def test_a_local_counter_behind_the_published_one_cannot_go_backwards(self):
        pages = published_stable("3.7.7.0")
        got = resolve(SEEDS, "experimental", fetcher(pages), seed="3.7.0", build=0)
        self.assertEqual(got["release"], "3.7.8.0")

    def test_published_map_comes_from_the_maximum_release(self):
        pages = published_stable("3.6.0.0")
        published = resolve(SEEDS, "stable", fetcher(pages), seed="3.5.0", build=0)["published"]
        self.assertEqual(published[("compat", "3.3.0")], {"sha256": "c" * 64, "size": 10})
        self.assertEqual(set(published), {(c, v) for c, v in SEEDS.items()})

    # An architecture that was never published has no bytes to reuse.
    def test_a_null_arch_contributes_nothing_to_the_published_map(self):
        pages = published_stable("3.6.0.0")
        published = resolve(SEEDS, "stable", fetcher(pages), seed="3.5.0", build=0)["published"]
        self.assertTrue(all("sha256" in e for e in published.values()))

    # The pointer exists but the file it names does not: that is a broken channel, not an empty one.
    def test_a_latest_naming_a_missing_release_file_raises(self):
        pages = {"client/releases/stable/latest": "3.6.0.0\n"}
        with self.assertRaises(NotFound):
            resolve(SEEDS, "stable", fetcher(pages), seed="3.5.0", build=0)


if __name__ == "__main__":
    unittest.main()
