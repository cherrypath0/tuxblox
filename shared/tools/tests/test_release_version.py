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


import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from release_version import (component_fields, derive_release, format_release, largest_bump,
                             next_build, parse_release, release_sort_key)


class LargestBump(unittest.TestCase):
    # The case that killed the summing scheme: a major bump on a component that
    # does not hold the maximum still has to move the release forward.
    def test_non_max_major_bump_still_advances(self):
        old = {"launcher": "4.0.0", "installer": "3.1.0", "libtuxblox": "3.2.1",
               "studio-mcp": "1.0.4", "compat": "4.0.0"}
        new = dict(old, installer="4.0.0")
        self.assertEqual(largest_bump(old, new), "major")
        self.assertGreater(release_sort_key(derive_release("4.3.5.0", "major", 4, 5, "experimental")),
                           release_sort_key("4.3.5.0"))

    def test_largest_bump_takes_the_largest_not_the_sum(self):
        old = {"launcher": "3.5.0", "compat": "3.5.0"}
        self.assertEqual(largest_bump(old, dict(old, launcher="3.6.0", compat="3.5.1")), "minor")
        self.assertEqual(largest_bump(old, dict(old, compat="3.5.1")), "patch")
        self.assertEqual(largest_bump(old, old), "none")

    def test_new_component_counts_as_a_minor(self):
        self.assertEqual(
            largest_bump({"launcher": "3.5.0"}, {"launcher": "3.5.0", "bootstrapper": "1.0.0"}),
            "minor")


class DeriveRelease(unittest.TestCase):
    """MAJOR.MINOR.BUILD.PATCH, worked out together because PATCH depends on whether BUILD moved."""

    def test_a_patch_only_release_holds_build_and_advances_patch(self):
        # The case the whole field order exists for: patches land under the build they apply to.
        self.assertEqual(derive_release("3.7.7.0", "patch", 3, 7, "experimental"), "3.7.7.1")
        self.assertEqual(derive_release("3.7.7.1", "patch", 3, 7, "experimental"), "3.7.7.2")

    def test_anything_more_than_a_patch_advances_build_and_resets_patch(self):
        self.assertEqual(derive_release("3.7.7.2", "minor", 3, 7, "experimental"), "3.8.8.0")
        self.assertEqual(derive_release("3.7.7.2", "none", 3, 7, "experimental"), "3.7.8.0")

    def test_major_resets_minor_and_patch_but_never_build(self):
        self.assertEqual(derive_release("3.9.2.7", "major", 4, 9, "experimental"), "4.0.10.0")
        # Even with nothing else moving, BUILD carries straight on across a MAJOR rise.
        self.assertEqual(derive_release("3.9.2.7", "none", 4, 9, "experimental"), "4.0.10.0")

    def test_only_experimental_advances_build(self):
        for channel in ("stable", "canary"):
            self.assertEqual(derive_release("3.7.7.0", "minor", 3, 7, channel), "3.8.7.0")
            self.assertEqual(derive_release("3.7.7.0", "patch", 3, 7, channel), "3.7.7.1")

    def test_nothing_published_starts_the_majors_line(self):
        self.assertEqual(derive_release(None, "minor", 3, 0, "experimental"), "3.0.1.0")

    # The monotonicity guarantee rests on component majors only rising.
    def test_a_regressing_major_is_rejected(self):
        with self.assertRaises(ValueError):
            derive_release("4.1.0.0", "patch", 3, 0, "experimental")

    def test_a_three_field_floor_is_advanced_past(self):
        self.assertGreater(release_sort_key(derive_release("3.5.0", "patch", 3, 1, "experimental")),
                           release_sort_key("3.5.0"))


class NextBuild(unittest.TestCase):
    """BUILD counts builds, only ever rises, and nothing resets it."""

    def test_every_experimental_build_gets_its_own_number(self):
        seen, build = [], 0
        for _ in range(3):
            build = next_build(build, "minor", "experimental")
            seen.append(build)
        self.assertEqual(seen, [1, 2, 3])

    def test_a_patch_only_release_does_not_count_as_a_build(self):
        self.assertEqual(next_build(9, "patch", "experimental"), 9)

    def test_promoting_to_another_channel_does_not_count_as_a_build(self):
        for channel in ("stable", "canary"):
            self.assertEqual(next_build(9, "minor", channel), 9)
            self.assertEqual(next_build(9, "major", channel), 9)

    # This is the difference from the scheme before it: a MAJOR rise used to send BUILD to zero.
    def test_nothing_resets_it(self):
        self.assertEqual(next_build(42, "major", "experimental"), 43)
        self.assertEqual(next_build(0, "major", "experimental"), 1)

    def test_a_bad_bump_level_is_rejected(self):
        with self.assertRaises(ValueError):
            next_build(1, "enormous", "experimental")


class NeverGoesBackwards(unittest.TestCase):
    """The invariant the whole scheme rests on: a derived release never sorts below the one it was
    derived from. It is easy to break, and it was broken once -- "nothing changed" on a channel
    that does not advance BUILD used to reset PATCH as well and derive a LOWER release."""

    def test_no_combination_derives_a_lower_release(self):
        channels = ("experimental", "canary", "stable")
        maxima = ("3.0.0.0", "3.7.0.2", "3.7.7.0", "3.7.7.9", "3.9.2.7", "4.0.10.0")
        for maximum in maxima:
            pmajor, _pminor, pbuild, _ppatch = parse_release(maximum)
            for bump in ("none", "patch", "minor", "major"):
                for channel in channels:
                    for major in (pmajor, pmajor + 1):
                        for build in (0, pbuild, pbuild + 5):
                            got = derive_release(maximum, bump, major, build, channel)
                            self.assertGreaterEqual(
                                release_sort_key(got), release_sort_key(maximum),
                                "%s + %s on %s (major %d, build %d) derived %s"
                                % (maximum, bump, channel, major, build, got))

    def test_nothing_changed_on_a_held_channel_repeats_the_maximum(self):
        # Not a lower number and not a new one: the same work, published again.
        self.assertEqual(derive_release("3.7.7.3", "none", 3, 7, "stable"), "3.7.7.3")


class Parsing(unittest.TestCase):
    """A release and a component disagree about the third field, so they parse separately."""

    def test_malformed_version_raises(self):
        for bad in ("", "3.5", "3.5.x", "3.5.0-stable"):
            with self.assertRaises(ValueError):
                parse_release(bad)

    def test_release_names_build_then_patch(self):
        self.assertEqual(parse_release("3.7.7.1"), (3, 7, 7, 1))
        self.assertEqual(parse_release("3.5.0"), (3, 5, 0, 0))

    def test_component_is_three_fields_and_rejects_a_release(self):
        self.assertEqual(component_fields("3.5.2"), (3, 5, 2))
        with self.assertRaises(ValueError):
            component_fields("3.5.2.1")

    def test_sort_key_orders_field_by_field(self):
        self.assertGreater(release_sort_key("3.7.8.0"), release_sort_key("3.7.7.9"))
        self.assertGreater(release_sort_key("3.7.7.1"), release_sort_key("3.7.7.0"))
        self.assertGreater(release_sort_key("3.8.0.0"), release_sort_key("3.7.99.99"))

    def test_format_round_trips(self):
        self.assertEqual(format_release(*parse_release("3.7.7.1")), "3.7.7.1")


if __name__ == "__main__":
    unittest.main()
