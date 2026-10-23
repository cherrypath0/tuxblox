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
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from provenance import read_provenance, write_provenance
from stage_release import build_source


class Provenance(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="tuxblox-prov-")
        self.addCleanup(shutil.rmtree, self.dir, ignore_errors=True)
        self.path = os.path.join(self.dir, ".provenance")

    def test_round_trips(self):
        write_provenance(self.path, "abc123", True, "3.6.0.0")
        self.assertEqual(read_provenance(self.path), ("abc123", True, "3.6.0.0"))

    def test_round_trips_a_clean_tree(self):
        write_provenance(self.path, "abc123", False, "3.6.0.1")
        self.assertEqual(read_provenance(self.path), ("abc123", False, "3.6.0.1"))

    # A build made before this change wrote two lines; its release is unknown, not wrong.
    def test_reads_a_pre_change_two_line_file(self):
        with open(self.path, "w") as f:
            f.write("abc123\nfalse\n")
        self.assertEqual(read_provenance(self.path), ("abc123", False, None))

    # Absent means the build predates the file entirely, which must read as unknown AND dirty so
    # the deploy gate refuses it rather than publishing an unattributable build.
    def test_a_missing_file_is_unknown_and_dirty(self):
        self.assertEqual(read_provenance(self.path), ("", True, None))


class BuildSource(unittest.TestCase):
    """Staging must attribute a release to the commit the BUILD was made from, never to HEAD at
    stage time: --stage-only republishes an older build/, and deploy.sh's licensing gate exists to
    stop a stale build/ being published as though it came from HEAD."""

    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="tuxblox-src-")
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        os.makedirs(os.path.join(self.root, "build"))

    def test_uses_the_commit_recorded_at_build_time(self):
        write_provenance(os.path.join(self.root, "build", ".provenance"),
                         "buildtime0000000000000000000000000000000", False, "3.6.0.0")
        self.assertEqual(build_source(self.root),
                         ("buildtime0000000000000000000000000000000", False))

    # A build older than this change recorded no release but did record its commit.
    def test_reads_a_two_line_provenance(self):
        with open(os.path.join(self.root, "build", ".provenance"), "w") as f:
            f.write("oldbuild\ntrue\n")
        self.assertEqual(build_source(self.root), ("oldbuild", True))

    # No provenance at all means the build predates the file: unknown, and dirty so the gate refuses it.
    def test_no_provenance_is_unknown_and_dirty(self):
        self.assertEqual(build_source(self.root), ("", True))


if __name__ == "__main__":
    unittest.main()
