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

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from analyze_fingerprint_trace import parse_maps, resolve


class TestParseMaps(unittest.TestCase):
    def test_parses_a_named_executable_range(self):
        ranges = parse_maps([
            "7f0000001000-7f0000002000 r-xp 00003000 08:02 1234 /path/to/foo.dll",
        ])
        self.assertEqual(len(ranges), 1)
        r = ranges[0]
        self.assertEqual(r.start, 0x7F0000001000)
        self.assertEqual(r.end, 0x7F0000002000)
        self.assertEqual(r.perms, "r-xp")
        self.assertEqual(r.file_offset, 0x3000)
        self.assertEqual(r.path, "/path/to/foo.dll")

    def test_parses_an_anonymous_range(self):
        ranges = parse_maps([
            "7f0000005000-7f0000006000 rwxp 00000000 00:00 0 ",
        ])
        self.assertEqual(len(ranges), 1)
        self.assertEqual(ranges[0].path, "")

    def test_parses_lines_behind_wines_trace_prefix(self):
        # This is the form the tracer actually emits. Wine prefixes every
        # trace line with "trace:<channel>:<function> ", so a parser anchored
        # at column 0 silently returns zero ranges against real trace output
        # while still passing on hand-written fixtures.
        ranges = parse_maps([
            "trace:tuxblox:dump_maps_once MAPS "
            "7f0000001000-7f0000002000 r-xp 00003000 08:02 1234 /path/to/foo.dll",
        ])
        self.assertEqual(len(ranges), 1)
        self.assertEqual(ranges[0].file_offset, 0x3000)
        self.assertEqual(ranges[0].path, "/path/to/foo.dll")

    def test_ignores_garbage_lines(self):
        ranges = parse_maps(["not a maps line", "", "REC seq=1 tid=0024"])
        self.assertEqual(ranges, [])


class TestResolve(unittest.TestCase):
    def test_rva_uses_file_offset_not_range_start(self):
        # A DLL's r-xp segment is NOT necessarily mapped at file offset 0.
        # RVA must be file_offset + (addr - start), never (addr - start).
        # Getting this wrong produced a false negative during the item 6
        # stack-dump investigation.
        ranges = parse_maps([
            "7f0000001000-7f0000002000 r-xp 00003000 08:02 1234 /path/to/foo.dll",
        ])
        self.assertEqual(resolve(0x7F0000001500, ranges),
                         ("/path/to/foo.dll", 0x3500))

    def test_anonymous_regions_report_as_anonymous(self):
        ranges = parse_maps([
            "7f0000005000-7f0000006000 rwxp 00000000 00:00 0 ",
        ])
        self.assertEqual(resolve(0x7F0000005010, ranges), ("<anonymous>", 0x10))

    def test_address_outside_every_range_is_unresolved(self):
        ranges = parse_maps([
            "7f0000001000-7f0000002000 r-xp 00003000 08:02 1234 /path/to/foo.dll",
        ])
        self.assertIsNone(resolve(0xDEADBEEF, ranges))

    def test_end_address_is_exclusive(self):
        ranges = parse_maps([
            "7f0000001000-7f0000002000 r-xp 00000000 08:02 1234 /path/to/foo.dll",
        ])
        self.assertIsNone(resolve(0x7F0000002000, ranges))


if __name__ == "__main__":
    unittest.main()
