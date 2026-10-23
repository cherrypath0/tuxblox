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

from bump_check import components_touched, guard


class ComponentsTouched(unittest.TestCase):
    def test_shared_ui_touches_three_components_not_studio_mcp(self):
        self.assertEqual(components_touched(["shared/ui/src/adw_look.cpp"]),
                         {"launcher", "installer", "libtuxblox"})

    # studio-mcp compiles six of the launcher's sources rather than copying them.
    def test_a_shared_launcher_source_touches_studio_mcp(self):
        self.assertIn("studio-mcp", components_touched(["launcher/src/launch_paths.cpp"]))
        self.assertIn("studio-mcp", components_touched(["launcher/src/versions_manifest.h"]))
        self.assertNotIn("studio-mcp", components_touched(["launcher/src/ui_adw/page_home.cpp"]))

    def test_a_launcher_only_change_touches_only_the_launcher(self):
        self.assertEqual(components_touched(["launcher/src/ui_adw/page_home.cpp"]), {"launcher"})

    def test_patches_and_submodules_touch_compat(self):
        self.assertEqual(components_touched(["compat/patches/dxvk/src/foo.c"]), {"compat"})
        self.assertEqual(components_touched(["compat/wine/dlls/ntdll/unix/system.c"]), {"compat"})

    def test_ui_tooling_touches_libtuxblox(self):
        self.assertIn("libtuxblox", components_touched(["shared/tools/ui-package.sh"]))

    def test_an_unrelated_path_touches_nothing(self):
        self.assertEqual(components_touched(["README.md", "workspace/notes.md"]), set())


class Guard(unittest.TestCase):
    def test_refuses_on_a_clean_main_with_a_missed_bump(self):
        verdict, lines = guard({"launcher"}, set(), dirty=False, branch="main")
        self.assertEqual(verdict, "refuse")
        self.assertTrue(any("launcher" in l for l in lines))

    def test_warns_on_a_dirty_tree_with_the_same_miss(self):
        self.assertEqual(guard({"launcher"}, set(), dirty=True, branch="main")[0], "warn")

    def test_warns_off_main(self):
        self.assertEqual(guard({"launcher"}, set(), dirty=False, branch="roblox-player")[0], "warn")

    def test_ok_when_the_version_moved(self):
        self.assertEqual(guard({"launcher"}, {"launcher"}, dirty=False, branch="main")[0], "ok")

    def test_reports_a_version_that_moved_with_no_change(self):
        verdict, lines = guard(set(), {"studio-mcp"}, dirty=False, branch="main")
        self.assertEqual(verdict, "warn")
        self.assertTrue(any("studio-mcp" in l for l in lines))

    def test_ok_when_nothing_changed_and_nothing_moved(self):
        self.assertEqual(guard(set(), set(), dirty=False, branch="main"), ("ok", []))


if __name__ == "__main__":
    unittest.main()
