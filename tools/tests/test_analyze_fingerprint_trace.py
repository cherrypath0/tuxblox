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

from analyze_fingerprint_trace import (build_report, parse_maps,
                                       parse_records, resolve)


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


class TestParseRecords(unittest.TestCase):
    def test_parses_a_record_behind_wines_trace_prefix(self):
        recs = parse_records([
            "trace:tuxblox:tuxblox_trace_record REC seq=1 tid=0024 "
            "surface=NtOpenKeyEx rip=0x7f1234567890 "
            "detail=\\Registry\\Machine\\Software\\Wine",
        ])
        self.assertEqual(len(recs), 1)
        r = recs[0]
        self.assertEqual(r.seq, 1)
        self.assertEqual(r.tid, "0024")
        self.assertEqual(r.surface, "NtOpenKeyEx")
        self.assertEqual(r.rip, 0x7F1234567890)
        self.assertEqual(r.detail, "\\Registry\\Machine\\Software\\Wine")

    def test_detail_may_be_empty(self):
        recs = parse_records([
            "REC seq=7 tid=00a4 surface=NtQueryObject rip=0x400000 detail=",
        ])
        self.assertEqual(recs[0].detail, "")

    def test_detail_may_contain_spaces(self):
        recs = parse_records([
            "REC seq=2 tid=0024 surface=NtOpenKeyEx rip=0x1000 detail=a b c",
        ])
        self.assertEqual(recs[0].detail, "a b c")

    def test_non_record_lines_are_ignored(self):
        recs = parse_records(["MAPS 7f00-7f01 r-xp 0 08:02 1 /x", "noise"])
        self.assertEqual(recs, [])


class TestBuildReport(unittest.TestCase):
    def setUp(self):
        self.ranges = parse_maps([
            "7f0000001000-7f0000002000 r-xp 00003000 08:02 1 /x/hyperion.dll",
            "7f0000009000-7f000000a000 rwxp 00000000 00:00 0 ",
        ])

    def test_groups_by_calling_module_and_counts_surfaces(self):
        recs = parse_records([
            "REC seq=1 tid=0024 surface=NtOpenKeyEx rip=0x7f0000001100 detail=k1",
            "REC seq=2 tid=0024 surface=NtOpenKeyEx rip=0x7f0000001100 detail=k2",
            "REC seq=3 tid=0024 surface=NtQueryObject rip=0x7f0000001200 detail=",
        ])
        report = build_report(recs, self.ranges)
        self.assertIn("/x/hyperion.dll", report)
        self.assertIn("NtOpenKeyEx", report)
        self.assertIn("2", report)

    def test_reports_first_touch_order(self):
        recs = parse_records([
            "REC seq=5 tid=0024 surface=NtQueryObject rip=0x7f0000001100 detail=",
            "REC seq=9 tid=0024 surface=NtOpenKeyEx rip=0x7f0000001100 detail=",
        ])
        report = build_report(recs, self.ranges)
        self.assertLess(report.index("NtQueryObject"), report.index("NtOpenKeyEx"))

    def test_unresolved_addresses_do_not_crash_the_report(self):
        recs = parse_records([
            "REC seq=1 tid=0024 surface=NtOpenKeyEx rip=0xdeadbeef detail=",
        ])
        report = build_report(recs, self.ranges)
        self.assertIn("<unresolved>", report)

    def test_anonymous_jit_callers_are_labelled(self):
        recs = parse_records([
            "REC seq=1 tid=0024 surface=NtOpenKeyEx rip=0x7f0000009010 detail=",
        ])
        self.assertIn("<anonymous>", build_report(recs, self.ranges))

    def test_ranking_follows_seq_not_log_line_order(self):
        # The tracer allocates seq with InterlockedIncrement and writes the
        # line as a separate step, so a multi-threaded process can emit lines
        # out of seq order.
        #
        # This bug only bites when a surface RECURS and a later-emitted line
        # carries a smaller seq -- with one record per surface, capturing
        # first_seq at encounter time is accidentally correct. Here
        # NtQueryObject's true first touch is seq=1, but seq=10 is emitted
        # first, so encounter-order tracking mis-ranks it after NtOpenKeyEx.
        recs = parse_records([
            "REC seq=10 tid=0031 surface=NtQueryObject rip=0x7f0000001100 detail=",
            "REC seq=5 tid=0024 surface=NtOpenKeyEx rip=0x7f0000001100 detail=",
            "REC seq=1 tid=0024 surface=NtQueryObject rip=0x7f0000001100 detail=",
        ])
        report = build_report(recs, self.ranges)
        self.assertLess(report.index("NtQueryObject"),
                        report.index("NtOpenKeyEx"))

    def test_cross_module_first_touch_ordering(self):
        # Modules themselves must be ordered by first-touch sequence, not
        # just surfaces within a module. Verify with records that touch
        # the anonymous region before the named module.
        ranges = parse_maps([
            "7f0000001000-7f0000002000 r-xp 00003000 08:02 1 /x/hyperion.dll",
            "7f0000009000-7f000000a000 rwxp 00000000 00:00 0 ",
        ])
        recs = parse_records([
            "REC seq=1 tid=0024 surface=NtQueryObject rip=0x7f0000009010 detail=",
            "REC seq=2 tid=0024 surface=NtOpenKeyEx rip=0x7f0000001100 detail=",
        ])
        report = build_report(recs, ranges)
        # <anonymous> (seq=1) should appear before /x/hyperion.dll (seq=2)
        self.assertLess(report.index("<anonymous>"), report.index("/x/hyperion.dll"))


if __name__ == "__main__":
    unittest.main()
