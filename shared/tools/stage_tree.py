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


def _from_tarball(ui_tarball, name):
    done = subprocess.run(["tar", "--zstd", "-xOf", ui_tarball, name], capture_output=True, text=True)
    return done.stdout.strip() if done.returncode == 0 else None


def _check_stack_source(build_dir, ui_tarball, version):
    """The stack tarball must be the one build/libtuxblox was unpacked from, and must contain the version it is about to be published under."""
    unpacked = _require(build_dir, "libtuxblox/LIBADWAITA_COMMIT", "libtuxblox")
    with open(unpacked) as f:
        if _from_tarball(ui_tarball, "LIBADWAITA_COMMIT") != f.read().strip():
            raise RuntimeError("libtuxblox: the stack tarball's LIBADWAITA_COMMIT is not the one "
                               "build/libtuxblox was unpacked from, so this build cannot be published")

    # The tarball is copied rather than repacked, so its contents are whatever the last stack build
    # produced. Publishing it under a version it does not itself contain cannot be undone: component
    # bytes are immutable at a published version, so that version would be unfixable.
    inside = _from_tarball(ui_tarball, "VERSION")
    if inside is None:
        raise RuntimeError("libtuxblox: the stack tarball carries no VERSION, so it predates "
                           "per-component versions -- rebuild the interface stack before publishing")
    if inside != version:
        raise RuntimeError("libtuxblox: the stack tarball says it is %s but it would be published "
                           "as %s -- rebuild the interface stack, or put LIBTUXBLOX_VERSION back to %s"
                           % (inside, version, inside))


def _pack_layer(build_dir, dest):
    # Contents at the archive ROOT, no wrapper directory: the installer extracts into
    # installDir/<filename>, so a wrapper would nest the payload one level too deep.
    subprocess.run(["tar", "--zstd", "-cf", dest, "-C", os.path.join(build_dir, "compat"), "."], check=True)


def stage(build_dir, out_dir, channel, release, versions, published, ui_tarball=None,
          source_commit=None, source_dirty=True, changelogs=None):
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
    _check_stack_source(build_dir, ui_tarball, versions["libtuxblox"])

    components = {}
    for component, (filename, _) in _ARTIFACTS.items():
        version = versions[component]
        arches = {arch: None for arch in _ARCHES}

        known = published.get((component, version))
        if known is not None:
            # Its bytes are already published at this version, and a component version's bytes are
            # packed exactly once, ever: tar records mtimes, so repacking identical contents would
            # change the sha256 and defeat the client's "skip what I already have". Nothing is
            # written, not even the directory, so a sync cannot publish an empty artifact.
            arches["x86_64"] = dict(known)
        else:
            dest_dir = os.path.join(out_dir, component, version, "x86_64")
            os.makedirs(dest_dir, exist_ok=True)
            dest = os.path.join(dest_dir, filename)
            if component == "compat":
                _pack_layer(build_dir, dest)
            elif component == "libtuxblox":
                shutil.copy2(ui_tarball, dest)
            else:
                shutil.copy2(os.path.join(build_dir, filename), dest)
            arches["x86_64"] = {"sha256": _digest(dest), "size": os.path.getsize(dest)}
            # Beside the component rather than inside the arch folder: the account of what changed
            # is the same whichever architecture you downloaded.
            changelog = (changelogs or {}).get(component)
            if changelog:
                shutil.copy2(changelog, os.path.join(os.path.dirname(dest_dir), "changelogs.md"))

        components[component] = {"version": version, "arch": arches}

    # deploy.sh refuses a release whose source is not clean, equal to HEAD and pushed to every
    # remote, because a published binary whose source nobody can fetch is a licence violation.
    # Unknown reads as dirty so an unattributable build is refused rather than published.
    content = {"release": release, "channel": channel, "components": components,
               # Which changelog the site should render: a merged per-component view, or the single
               # {release}.md that backfilled historical releases carry instead.
               "changelog": "components",
               "source_commit": source_commit or None, "source_dirty": bool(source_dirty)}

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
