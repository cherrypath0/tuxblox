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
import subprocess
import sys
import tempfile
import unittest

TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
CLI = os.path.join(TOOLS, "stage_release.py")


class Cli(unittest.TestCase):
    """main() has no unit coverage, and a NameError inside it passed both the unit tests and a
    compile check. These run the real entry point, which is the only thing that catches that."""

    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="tuxblox-cli-")
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        for name, version in (("launcher", "3.5.0"), ("compat", "3.3.0"),
                              ("installer", "3.1.0"), ("studio-mcp", "2.0.1")):
            os.makedirs(os.path.join(self.root, name))
            with open(os.path.join(self.root, name, "VERSION"), "w") as f:
                f.write(version + "\n")
        os.makedirs(os.path.join(self.root, "shared", "tools"))
        with open(os.path.join(self.root, "shared", "tools", "ui-versions.env"), "w") as f:
            f.write("UI_STACK_VERSION=3\nLIBTUXBLOX_VERSION=3.1.0\n")
        os.makedirs(os.path.join(self.root, "build"))

    def run_cli(self, **env):
        full = dict(os.environ, **env)
        return subprocess.run([sys.executable, CLI, "--root", self.root,
                               "--channel", "stable", "--seed", "3.5.0"],
                              capture_output=True, text=True, env=full)

    # Nothing is published and the host is unreachable: it must refuse with a sentence, not a stack.
    def test_an_unreachable_host_refuses_without_a_traceback(self):
        done = self.run_cli(TUXBLOX_SETUP_URL="https://no-such-host.invalid")
        self.assertEqual(done.returncode, 1)
        self.assertIn("!!", done.stderr)
        self.assertNotIn("Traceback", done.stderr)

    def test_a_malformed_component_version_names_the_component(self):
        with open(os.path.join(self.root, "installer", "VERSION"), "w") as f:
            f.write("\n")
        done = self.run_cli(TUXBLOX_SETUP_URL="https://no-such-host.invalid")
        self.assertEqual(done.returncode, 1)
        self.assertIn("installer", done.stderr)
        self.assertNotIn("Traceback", done.stderr)

    # An explicit release is the offline escape hatch, and must get past resolution.
    def test_an_explicit_release_survives_an_unreachable_host(self):
        done = subprocess.run([sys.executable, CLI, "--root", self.root, "--channel", "stable",
                               "--seed", "3.5.0", "--release", "3.9.0.0"],
                              capture_output=True, text=True,
                              env=dict(os.environ, TUXBLOX_SETUP_URL="https://no-such-host.invalid"))
        # It gets past resolution and fails on the missing build tree instead, naming a component.
        self.assertNotIn("Traceback", done.stderr)
        self.assertIn("!!", done.stderr)


if __name__ == "__main__":
    unittest.main()
