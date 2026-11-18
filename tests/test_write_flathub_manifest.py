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
import subprocess
import sys
import tempfile
import unittest

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "..", "tools", "write-flathub-manifest.py")
IN_TREE = os.path.join(HERE, "..", "packaging", "flatpak", "net.tuxblox.TuxBlox.yml")
SHA = "0" * 64


class WriteFlathubManifest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.out = os.path.join(self.dir.name, "net.tuxblox.TuxBlox.yml")
        self.addCleanup(self.dir.cleanup)

    def run_script(self, version="3.12.15.1"):
        done = subprocess.run([sys.executable, SCRIPT, "--version", version,
                               "--out", self.out, "--sha256", SHA],
                              capture_output=True, text=True)
        written = None
        if os.path.exists(self.out):
            with open(self.out, encoding="utf-8") as handle:
                written = yaml.safe_load(handle)
        return done.returncode, done.stderr, written

    def test_the_dir_source_becomes_a_pinned_archive(self):
        code, err, out = self.run_script()
        self.assertEqual(code, 0, err)
        sources = out["modules"][0]["sources"]
        self.assertEqual(len(sources), 1)
        self.assertEqual(sources[0]["type"], "archive")
        self.assertTrue(sources[0]["url"].endswith("/refs/tags/3.12.15.1.tar.gz"), sources[0]["url"])
        self.assertEqual(sources[0]["sha256"], SHA)

    def test_the_checker_follows_the_stable_channel_only(self):
        _, err, out = self.run_script()
        checker = out["modules"][0]["sources"][0]["x-checker-data"]
        self.assertEqual(checker["type"], "json")
        self.assertIn("/client/stable/latest", checker["url"], err)
        self.assertEqual(checker["version-query"], ".release")
        self.assertTrue(checker["is-main-source"])

    def test_the_baked_channel_is_stable(self):
        _, _, out = self.run_script()
        self.assertEqual(out["modules"][0]["build-options"]["env"]["TUXBLOX_CHANNEL"], "stable")

    def test_the_release_env_is_the_pinned_version(self):
        _, err, out = self.run_script(version="3.12.15.1")
        self.assertEqual(out["modules"][0]["build-options"]["env"]["TUXBLOX_RELEASE"], "3.12.15.1", err)

    def test_a_version_with_no_metainfo_entry_is_refused(self):
        code, err, out = self.run_script(version="9.9.9.9")
        self.assertNotEqual(code, 0)
        self.assertIsNone(out)
        self.assertIn("9.9.9.9", err)

    def test_everything_else_is_carried_over_unchanged(self):
        with open(IN_TREE, encoding="utf-8") as handle:
            source = yaml.safe_load(handle)
        _, err, out = self.run_script()
        self.assertIsNotNone(out, err)
        for key in ("app-id", "runtime", "runtime-version", "sdk", "command",
                    "add-extensions", "finish-args"):
            self.assertEqual(out[key], source[key], key)
        self.assertEqual(out["modules"][0]["build-commands"], source["modules"][0]["build-commands"])
        self.assertEqual(out["modules"][0]["buildsystem"], source["modules"][0]["buildsystem"])

    def test_the_comments_explaining_each_permission_survive(self):
        self.run_script()
        with open(self.out, encoding="utf-8") as handle:
            written = handle.read()
        self.assertIn("# Required for audio to play", written)
        self.assertIn("/dev/ntsync needs this", written)


if __name__ == "__main__":
    unittest.main()
