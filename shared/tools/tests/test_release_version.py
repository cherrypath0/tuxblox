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

from release_version import largest_bump, derive_release, release_sort_key


class LargestBump(unittest.TestCase):
    # The case that killed the summing scheme: a major bump on a component that
    # does not hold the maximum still has to move the release forward.
    def test_non_max_major_bump_still_advances(self):
        old = {"launcher": "4.0.0", "installer": "3.1.0", "libtuxblox": "3.2.1",
               "studio-mcp": "1.0.4", "compat": "4.0.0"}
        new = dict(old, installer="4.0.0")
        self.assertEqual(largest_bump(old, new), "major")
        self.assertGreater(release_sort_key(derive_release("4.3.5.0", "major", 4)),
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
    def test_build_increments_within_one_mmp_and_resets_across(self):
        self.assertEqual(derive_release("3.6.0.2", "none", 3), "3.6.0.3")
        self.assertEqual(derive_release("3.6.0.9", "patch", 3), "3.6.1.0")
        self.assertEqual(derive_release("3.6.1.4", "minor", 3), "3.7.0.0")

    def test_major_comes_from_the_highest_component_major(self):
        self.assertEqual(derive_release("3.6.1.4", "major", 4), "4.0.0.0")

    def test_major_rise_resets_the_lower_fields(self):
        self.assertEqual(derive_release("3.9.2.7", "patch", 4), "4.0.0.0")
        self.assertEqual(derive_release("3.9.2.7", "none", 4), "4.0.0.0")

    def test_seeding_advances_past_the_three_field_floor(self):
        self.assertGreater(release_sort_key(derive_release("3.5.0", "patch", 3)),
                           release_sort_key("3.5.0"))

    # Nothing published yet: start the major's line rather than inventing a
    # floor here, so the repo's seed stays the caller's policy.
    def test_nothing_published_starts_the_majors_line(self):
        self.assertEqual(derive_release(None, "minor", 3), "3.0.0.0")

    # The spec's monotonicity guarantee rests on component majors only rising.
    def test_a_regressing_major_is_rejected(self):
        with self.assertRaises(ValueError):
            derive_release("4.1.0.0", "patch", 3)


class Parsing(unittest.TestCase):
    def test_malformed_version_raises(self):
        for bad in ("", "3.5", "3.5.x", "3.5.0-stable"):
            with self.assertRaises(ValueError):
                release_sort_key(bad)

    def test_three_and_four_fields_both_parse(self):
        self.assertEqual(release_sort_key("3.5.0"), (3, 5, 0, 0))
        self.assertEqual(release_sort_key("3.5.0.2"), (3, 5, 0, 2))


if __name__ == "__main__":
    unittest.main()
