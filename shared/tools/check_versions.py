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

"""Prints every component's version, or names the one that is unreadable. Run before a build."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from component_versions import read_component_versions

try:
    versions = read_component_versions(sys.argv[1])
except (IndexError, ValueError) as error:
    print("!! %s" % error, file=sys.stderr)
    raise SystemExit(1)

print(" ".join("%s %s" % (name, versions[name]) for name in sorted(versions)))
