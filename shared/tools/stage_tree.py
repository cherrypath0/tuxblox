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

"""Writes a release into the shape setup.tuxblox.net serves."""

import hashlib
import json
import os
import shutil
import subprocess

from component_versions import read_ui_env_value

# Component -> (the name it is served under, whether it is an archive).
_ARTIFACTS = {
    "launcher":   ("TuxBloxLauncher", False),
    "installer":  ("TuxBloxInstaller", False),
    "studio-mcp": ("studio-mcp", False),
    "compat":     ("compat.tar.zst", True),
    "libtuxblox": ("libtuxblox.tar.zst", True),
}

# Every architecture the layout reserves. Only the ones actually built get an entry; the rest are null.
_ARCHES = ("x86_64", "arm64")

# The library directory each architecture's interface stack unpacks into, which is what proves the
# stack is really there rather than an empty directory that would tar up fine and fail on a user's machine.
_LIB_TRIPLET = {"x86_64": "x86_64-linux-gnu", "arm64": "aarch64-linux-gnu"}


def _digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _require(build_dir, rel, component):
    path = os.path.join(build_dir, rel)
    if not os.path.exists(path):
        raise RuntimeError("%s: build/%s is missing, so this build cannot be published" % (component, rel))
    return path


def _check_stack_source(build_dir, ui_tarball):
    """The stack tarball must be the one build/libtuxblox was unpacked from, or the licence notice would name source for a different build."""
    inside = subprocess.run(["tar", "--zstd", "-xOf", ui_tarball, "LIBADWAITA_COMMIT"],
                            capture_output=True, text=True)
    unpacked = _require(build_dir, "libtuxblox/LIBADWAITA_COMMIT", "libtuxblox")
    with open(unpacked) as f:
        if inside.returncode != 0 or inside.stdout.strip() != f.read().strip():
            raise RuntimeError("libtuxblox: the stack tarball's LIBADWAITA_COMMIT is not the one "
                               "build/libtuxblox was unpacked from, so this build cannot be published")


def _pack_layer(build_dir, dest):
    # Contents at the archive ROOT, no wrapper directory: the installer extracts into
    # installDir/<filename>, so a wrapper would nest the payload one level too deep.
    subprocess.run(["tar", "--zstd", "-cf", dest, "-C", os.path.join(build_dir, "compat"), "."], check=True)


def stage(build_dir, out_dir, channel, release, versions, published, ui_tarball=None):
    """Writes the release file, `latest`, and every component the release does not already have published. Returns the release file's content."""
    if ui_tarball is None:
        # build/ sits beside shared/ in the repo, and the stack is an input to the build rather than output of it.
        root = os.path.dirname(os.path.abspath(build_dir))
        # Named by the stack version rather than globbed, so a stack being tested can sit beside the one releases ship.
        stack = read_ui_env_value(root, "UI_STACK_VERSION")
        ui_tarball = os.path.join(root, "shared", "ui", "dist",
                                  "ui-stack-%s-x86_64.tar.zst" % stack)

    _require(build_dir, "compat/main", "compat")
    _require(build_dir, "TuxBloxLauncher", "launcher")
    _require(build_dir, "TuxBloxInstaller", "installer")
    _require(build_dir, "studio-mcp", "studio-mcp")
    _require(build_dir, "libtuxblox/lib/" + _LIB_TRIPLET["x86_64"], "libtuxblox")
    _require(build_dir, "libtuxblox/share/icons/TuxBlox/index.theme", "libtuxblox")
    _check_stack_source(build_dir, ui_tarball)

    components = {}
    for component, (filename, _) in _ARTIFACTS.items():
        version = versions[component]
        arches = {arch: None for arch in _ARCHES}

        known = published.get((component, version))
        if known is not None:
            # Already published at this version, so its bytes exist and must not be packed a second
            # time: tar records mtimes, so a repack of identical contents hashes differently.
            arches["x86_64"] = dict(known)
        else:
            dest_dir = os.path.join(out_dir, "components", component, version, "x86_64")
            os.makedirs(dest_dir, exist_ok=True)
            dest = os.path.join(dest_dir, filename)
            if component == "compat":
                _pack_layer(build_dir, dest)
            elif component == "libtuxblox":
                shutil.copy2(ui_tarball, dest)
            else:
                shutil.copy2(os.path.join(build_dir, filename), dest)
            arches["x86_64"] = {"sha256": _digest(dest), "size": os.path.getsize(dest)}

        components[component] = {"version": version, "arch": arches}

    content = {"release": release, "channel": channel, "components": components}

    release_dir = os.path.join(out_dir, "releases", channel)
    os.makedirs(release_dir, exist_ok=True)
    with open(os.path.join(release_dir, release + ".json"), "w") as f:
        json.dump(content, f, indent=2, sort_keys=True)
        f.write("\n")
    # Written after the release file: `latest` naming a release whose file is not there yet is the
    # one ordering that breaks a client.
    with open(os.path.join(release_dir, "latest"), "w") as f:
        f.write(release + "\n")
    return content
