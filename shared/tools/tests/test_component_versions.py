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

from component_versions import read_component_versions

SEEDS = {"launcher": "3.5.0", "compat": "3.3.0", "installer": "3.1.0",
         "libtuxblox": "3.1.0", "studio-mcp": "2.0.1"}


class ReadComponentVersions(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="tuxblox-versions-")
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        for name in ("launcher", "compat", "installer", "studio-mcp"):
            self.write(os.path.join(name, "VERSION"), SEEDS[name] + "\n")
        self.write(os.path.join("shared", "tools", "ui-versions.env"),
                   "UI_STACK_VERSION=3\nLIBTUXBLOX_VERSION=%s\n" % SEEDS["libtuxblox"])

    def write(self, rel, text):
        path = os.path.join(self.root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)

    def test_reads_all_five(self):
        self.assertEqual(read_component_versions(self.root), SEEDS)

    def test_strips_trailing_whitespace(self):
        self.write("launcher/VERSION", "3.5.0\n\n  ")
        self.assertEqual(read_component_versions(self.root)["launcher"], "3.5.0")

    def test_missing_file_names_the_component(self):
        os.remove(os.path.join(self.root, "compat", "VERSION"))
        with self.assertRaisesRegex(ValueError, "compat"):
            read_component_versions(self.root)

    def test_empty_file_names_the_component(self):
        self.write("installer/VERSION", "\n")
        with self.assertRaisesRegex(ValueError, "installer"):
            read_component_versions(self.root)

    def test_channel_suffix_is_rejected(self):
        self.write("launcher/VERSION", "3.5.0-stable\n")
        with self.assertRaisesRegex(ValueError, "launcher"):
            read_component_versions(self.root)

    # libtuxblox has no VERSION file of its own: its version rides in the env
    # file beside UI_STACK_VERSION, which must not be mistaken for it.
    def test_missing_libtuxblox_version_names_it_and_ignores_the_stack_version(self):
        self.write("shared/tools/ui-versions.env", "UI_STACK_VERSION=3\n")
        with self.assertRaisesRegex(ValueError, "libtuxblox"):
            read_component_versions(self.root)


if __name__ == "__main__":
    unittest.main()
