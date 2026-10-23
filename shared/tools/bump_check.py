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

"""Catches a component whose sources moved but whose VERSION did not."""

# The six launcher sources studio-mcp compiles rather than keeping a second copy of, named
# explicitly from studio-mcp/CMakeLists.txt so adding a seventh there cannot silently widen this.
_MCP_SHARED_STEMS = ("install_paths", "launch_paths", "lnk_resolver",
                     "pe_version", "prefix_user", "versions_manifest")

# A component's inputs are not just its own directory: the interface stack is linked by the two
# graphical programs, and studio-mcp borrows from the launcher.
COMPONENT_INPUTS = {
    "launcher":   ("launcher/", "shared/ui/"),
    "installer":  ("installer/", "shared/ui/"),
    "studio-mcp": ("studio-mcp/",),
    "libtuxblox": ("shared/ui/", "shared/tools/ui-"),
    "compat":     ("compat/",),
}


def _is_mcp_shared(path):
    for stem in _MCP_SHARED_STEMS:
        if path in ("launcher/src/%s.cpp" % stem, "launcher/src/%s.h" % stem):
            return True
    # json.hpp is one of those six sources' dependencies, so it is studio-mcp's input too.
    return path.startswith("launcher/third_party/")


def components_touched(paths):
    """Every component whose inputs include at least one of `paths`."""
    touched = set()
    for path in paths:
        for component, prefixes in COMPONENT_INPUTS.items():
            if any(path.startswith(prefix) for prefix in prefixes):
                touched.add(component)
        if _is_mcp_shared(path):
            touched.add("studio-mcp")
    return touched


def guard(touched, versions_moved, dirty, branch):
    """A verdict of "ok", "warn" or "refuse", and the lines to print."""
    lines = []
    missed = sorted(touched - versions_moved)
    for component in missed:
        lines.append("%s changed since its version was last published, but %s's VERSION did not move"
                     % (component, component))
    for component in sorted(versions_moved - touched):
        lines.append("%s's VERSION moved but nothing it is built from changed" % component)
    if not lines:
        return "ok", []
    # A missed bump is the serious half, and it only refuses where a release could actually be cut
    # from: stage_release() runs on every plain build, and most of those are dirty or off main.
    if missed and not dirty and branch == "main":
        return "refuse", lines
    return "warn", lines
