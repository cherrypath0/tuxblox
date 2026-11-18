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
import re
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "..", "tools", "add-release-notes.py")

EMPTY_METAINFO = """<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>net.tuxblox.TuxBlox</id>
  <name>TuxBlox</name>
  <releases/>
</component>
"""


class AddReleaseNotes(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.root = self.dir.name
        self.metainfo = os.path.join(self.root, "net.tuxblox.TuxBlox.metainfo.xml")
        with open(self.metainfo, "w", encoding="utf-8") as handle:
            handle.write(EMPTY_METAINFO)
        self.addCleanup(self.dir.cleanup)

    def run_script(self, version, notes=None, extra=()):
        """Run the generator; return (returncode, stderr, the metainfo's text)."""
        args = [sys.executable, SCRIPT, version, "--metainfo", self.metainfo,
                "--root", self.root, "--date", "2026-11-18"]
        if notes is not None:
            args += ["--notes-file", "-"]
        args += list(extra)
        done = subprocess.run(args, input=notes, capture_output=True, text=True)
        with open(self.metainfo, encoding="utf-8") as handle:
            return done.returncode, done.stderr, handle.read()

    def write_component(self, name, version, notes):
        os.makedirs(os.path.join(self.root, name), exist_ok=True)
        with open(os.path.join(self.root, name, "changelogs.md"), "w", encoding="utf-8") as handle:
            handle.write(notes)
        with open(os.path.join(self.root, name, "VERSION"), "w", encoding="utf-8") as handle:
            handle.write(version + "\n")

    def test_bullets_become_appstream_list_items(self):
        code, err, text = self.run_script("1.0.0.0", "* Fixed a thing\n* Fixed another\n")
        self.assertEqual(code, 0, err)
        self.assertIn("<li>Fixed a thing</li>", text)
        self.assertIn("<li>Fixed another</li>", text)
        self.assertEqual(text.count("<ul>"), 1)

    def test_markup_is_converted_not_passed_through(self):
        code, err, text = self.run_script("1.0.0.0", "* Added the **Advanced** section and `tbxFlagFoo`\n")
        self.assertEqual(code, 0, err)
        self.assertIn("<em>Advanced</em>", text)
        self.assertIn("<code>tbxFlagFoo</code>", text)
        self.assertNotIn("**", text)
        self.assertNotIn("`", text)

    def test_xml_special_characters_are_escaped(self):
        code, err, text = self.run_script("1.0.0.0", "* Fixed a & b <c> in the log\n")
        self.assertEqual(code, 0, err)
        self.assertIn("Fixed a &amp; b &lt;c&gt; in the log", text)

    def test_a_nested_bullet_stays_in_the_list(self):
        code, err, text = self.run_script("1.0.0.0", "* Top level\n  * Nested detail\n")
        self.assertEqual(code, 0, err)
        self.assertIn("<li>Top level</li>", text)
        self.assertIn("<li>Nested detail</li>", text)

    def test_a_component_section_becomes_a_labelled_list(self):
        with open(os.path.join(self.root, "CHANGELOG.md"), "w", encoding="utf-8") as handle:
            handle.write("* The release's own note\n")
        self.write_component("launcher", "3.6.1", "* A launcher note\n")
        code, err, text = self.run_script("1.0.0.0")
        self.assertEqual(code, 0, err)
        self.assertIn("<li>The release's own note</li>", text)
        self.assertIn("<p>Launcher 3.6.1</p>", text)
        self.assertIn("<li>A launcher note</li>", text)

    def test_empty_sources_are_refused(self):
        with open(os.path.join(self.root, "CHANGELOG.md"), "w", encoding="utf-8") as handle:
            handle.write("\n")
        code, err, text = self.run_script("1.0.0.0")
        self.assertNotEqual(code, 0)
        self.assertNotIn("<release ", text)
        self.assertTrue(err.strip())
        self.assertNotIn("No such file", err)

    def test_an_existing_version_is_refused(self):
        code, err, _ = self.run_script("1.0.0.0", "* First\n")
        self.assertEqual(code, 0, err)
        code, err, text = self.run_script("1.0.0.0", "* Second\n")
        self.assertNotEqual(code, 0)
        self.assertEqual(len(re.findall(r'<release version="1\.0\.0\.0"', text)), 1)
        self.assertIn("First", text)
        self.assertNotIn("Second", text)

    def test_replace_rewrites_the_existing_entry(self):
        self.run_script("1.0.0.0", "* First\n")
        code, err, text = self.run_script("1.0.0.0", "* Second\n", extra=["--replace"])
        self.assertEqual(code, 0, err)
        self.assertEqual(len(re.findall(r'<release version="1\.0\.0\.0"', text)), 1)
        self.assertIn("Second", text)
        self.assertNotIn("First", text)

    def test_a_fenced_code_block_is_not_mangled(self):
        notes = "Run this:\n\n```bash\nflatpak install --user ./net.tuxblox.TuxBlox.flatpak\n```\n"
        code, err, text = self.run_script("1.0.0.0", notes)
        self.assertEqual(code, 0, err)
        self.assertIn("<code>flatpak install --user ./net.tuxblox.TuxBlox.flatpak</code>", text)
        self.assertNotIn("`", text)
        self.assertNotIn("bash", text)

    def test_an_autolink_loses_its_brackets(self):
        code, err, text = self.run_script("1.0.0.0", "* See <https://tuxblox.net/docs>\n")
        self.assertEqual(code, 0, err)
        self.assertIn("<li>See https://tuxblox.net/docs</li>", text)

    def test_a_numbered_item_becomes_an_ordered_list(self):
        code, err, text = self.run_script("1.0.0.0", "1. First\n2. Second\n")
        self.assertEqual(code, 0, err)
        self.assertIn("<ol>", text)
        self.assertIn("<li>First</li>", text)
        self.assertIn("<li>Second</li>", text)

    def test_a_paragraph_keeps_its_text_on_one_line(self):
        code, err, text = self.run_script("1.0.0.0", "**TuxBlox** is now available.\n")
        self.assertEqual(code, 0, err)
        self.assertIn("<p><em>TuxBlox</em> is now available.</p>", text)

    def test_the_newest_entry_is_first(self):
        self.run_script("1.0.0.0", "* Older\n")
        code, err, text = self.run_script("1.1.0.0", "* Newer\n")
        self.assertEqual(code, 0, err)
        versions = re.findall(r'<release version="([^"]+)"', text)
        self.assertEqual(versions, ["1.1.0.0", "1.0.0.0"])


if __name__ == "__main__":
    unittest.main()
