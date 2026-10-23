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

"""Reads each component's own version, the five numbers a release is derived from."""

import os

# The four that keep a VERSION file beside their sources.
_FILE_COMPONENTS = ("launcher", "compat", "installer", "studio-mcp")

# libtuxblox has no sources of its own to put a file beside, so its version rides in the interface stack's env file.
_UI_ENV = os.path.join("shared", "tools", "ui-versions.env")
_UI_KEY = "LIBTUXBLOX_VERSION"


def _validate(name, raw):
    version = (raw or "").strip()
    parts = version.split(".")
    if len(parts) != 3 or not all(p.isdigit() for p in parts):
        raise ValueError("%s: not a bare x.y.z version: %r" % (name, version))
    return version


def _read_file(root, name):
    path = os.path.join(root, name, "VERSION")
    try:
        with open(path) as f:
            return f.read()
    except OSError as e:
        raise ValueError("%s: cannot read %s (%s)" % (name, path, e))


def read_ui_env_value(repo_root, key):
    """One value out of the interface stack's env file, which is shell-sourced elsewhere and so is parsed rather than imported."""
    path = os.path.join(repo_root, _UI_ENV)
    try:
        with open(path) as f:
            lines = f.readlines()
    except OSError as e:
        raise ValueError("cannot read %s (%s)" % (path, e))
    for line in lines:
        name, _, value = line.partition("=")
        if name.strip() == key:
            return value.strip()
    raise ValueError("%s does not set %s" % (path, key))


def _read_ui_env(root):
    try:
        return read_ui_env_value(root, _UI_KEY)
    except ValueError as e:
        raise ValueError("libtuxblox: %s" % e)


def read_component_versions(repo_root):
    """Every component's version, keyed by component name. Raises ValueError naming the component on anything unreadable or malformed."""
    versions = {name: _validate(name, _read_file(repo_root, name)) for name in _FILE_COMPONENTS}
    versions["libtuxblox"] = _validate("libtuxblox", _read_ui_env(repo_root))
    return versions
