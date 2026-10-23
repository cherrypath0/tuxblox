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

"""Records which source a build was made from, and which release it became."""


def write_provenance(path, commit, dirty, release):
    with open(path, "w") as f:
        f.write("%s\n%s\n%s\n" % (commit, "true" if dirty else "false", release))


def read_provenance(path):
    """(commit, dirty, release). An absent file reads as unknown-and-dirty so the deploy gate refuses it, and a two-line file from before this change has an unknown release rather than a wrong one."""
    try:
        with open(path) as f:
            lines = [line.strip() for line in f.readlines()]
    except OSError:
        return "", True, None
    commit = lines[0] if lines else ""
    dirty = lines[1] == "true" if len(lines) > 1 else True
    release = lines[2] if len(lines) > 2 and lines[2] else None
    return commit, dirty, release
