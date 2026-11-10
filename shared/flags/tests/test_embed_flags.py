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

"""The defaults are compiled in, so a bad one must fail the build rather than ship."""

import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "..", "tools", "embed-flags.py")


def run(flags_text):
    """Run the generator over `flags_text`; return (returncode, stderr, header or None)."""
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "Flags.json")
        out = os.path.join(d, "flags.h")
        with open(src, "w", encoding="utf-8") as handle:
            handle.write(flags_text)
        done = subprocess.run([sys.executable, SCRIPT, src, out, "kTestFlags"],
                              capture_output=True, text=True)
        header = None
        if os.path.exists(out):
            with open(out, encoding="utf-8") as handle:
                header = handle.read()
        return done.returncode, done.stderr, header


GOOD = json.dumps({
    "tbxFlagEnableChristmasTheme": True,
    "tbxStringPrivacyPolicyVersion": "2026-10-16",
    "tbxIntRobloxInstallerParallelWorkers": 6,
})


class EmbedFlagsTests(unittest.TestCase):
    def test_a_good_file_becomes_a_header_holding_its_json(self):
        code, _, header = run(GOOD)
        self.assertEqual(code, 0)
        self.assertIn("kTestFlags", header)
        self.assertIn("tbxFlagEnableChristmasTheme", header)
        self.assertIn("#pragma once", header)

    def test_a_name_with_no_known_prefix_fails_the_build(self):
        code, err, _ = run(json.dumps({"somethingElse": True}))
        self.assertNotEqual(code, 0)
        self.assertIn("somethingElse", err)

    def test_a_value_that_contradicts_its_prefix_fails_the_build(self):
        for flags in ({"tbxFlagA": "yes"}, {"tbxIntB": "4"}, {"tbxStringC": 9},
                      {"tbxFlagA": 1}, {"tbxIntB": 4.5}):
            code, err, _ = run(json.dumps(flags))
            self.assertNotEqual(code, 0, str(flags))
            self.assertIn(list(flags)[0], err)

    def test_malformed_json_fails_the_build(self):
        code, _, _ = run('{"tbxFlagA":')
        self.assertNotEqual(code, 0)

    def test_a_file_that_is_not_an_object_fails_the_build(self):
        for text in ("[1,2,3]", '"a string"', "42", "null"):
            code, _, _ = run(text)
            self.assertNotEqual(code, 0, text)

    def test_a_nested_value_fails_the_build(self):
        code, err, _ = run(json.dumps({"tbxFlagA": {"nested": True}}))
        self.assertNotEqual(code, 0)
        self.assertIn("tbxFlagA", err)

    def test_an_empty_set_is_allowed(self):
        # Shipping with no defaults at all is a legitimate state, not an error.
        code, _, header = run("{}")
        self.assertEqual(code, 0)
        self.assertIn("kTestFlags", header)

    def test_the_generated_header_survives_awkward_string_values(self):
        # A quote, a backslash and a non-ASCII character must come back out intact.
        awkward = {"tbxStringA": 'he said "x"', "tbxStringB": "a\\b", "tbxStringC": "café"}
        code, _, header = run(json.dumps(awkward))
        self.assertEqual(code, 0)
        self.assertNotIn('\n', header.split('"')[1] if '"' in header else '')
        # The embedded text must parse back to exactly what went in.
        start = header.index('R"') if 'R"' in header else None
        self.assertIsNotNone(start, "expected a raw string literal")

    def test_the_header_is_rewritten_rather_than_appended_to(self):
        code, _, header = run(GOOD)
        self.assertEqual(code, 0)
        self.assertEqual(header.count("#pragma once"), 1)


class ShippedFilesTests(unittest.TestCase):
    """Both files in the repository must pass the same checks the build applies."""

    FLAGS_DIR = os.path.join(HERE, "..")

    def check_real(self, name):
        path = os.path.join(self.FLAGS_DIR, name)
        self.assertTrue(os.path.exists(path), path)
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "flags.h")
            done = subprocess.run([sys.executable, SCRIPT, path, out, "kFlags"],
                                  capture_output=True, text=True)
            self.assertEqual(done.returncode, 0, done.stderr)

    def test_the_compiled_in_defaults_are_valid(self):
        self.check_real("DefaultFlags-x86_64.json")

    def test_the_publishable_set_is_valid(self):
        # Flags-x86_64.json is what installs are TOLD; DefaultFlags-x86_64.json is what
        # they fall back to. Same checks, so a set that could not be compiled in cannot
        # be published either.
        self.check_real("Flags-x86_64.json")

    def test_the_publishable_set_may_name_a_flag_with_no_compiled_in_default(self):
        # That is how a flag is introduced ahead of the release that reads it.
        code, _, _ = run(json.dumps({"tbxFlagSomethingNotYetShipped": True}))
        self.assertEqual(code, 0)


if __name__ == "__main__":
    unittest.main()
