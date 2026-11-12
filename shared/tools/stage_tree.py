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

import datetime
import hashlib
import json
import os
import shutil
import subprocess

from component_versions import read_ui_env_value
from stamp_release import stamp

# Component -> (the name it is served under, whether it is an archive, and the binary inside the
# build that carries the release stamp). The interface stack is the one with no binary to stamp: it
# is GTK and libadwaita, built from source and copied as it was packed.
_ARTIFACTS = {
    "launcher":   ("TuxBloxLauncher", False, "TuxBloxLauncher"),
    "installer":  ("TuxBloxInstaller", False, "TuxBloxInstaller"),
    "studio-mcp": ("studio-mcp", False, "studio-mcp"),
    "compat":     ("compat.tar.zst", True, "compat/main"),
    "libtuxblox": ("libtuxblox.tar.zst", True, None),
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


def component_dir(out_dir, component, version, release, arch, legacy_paths):
    """Where this release's copy of a component is written.

    A release publishes its own copy of every component, because every binary carries the release it
    was published in. So one component version has a directory per release it appears in, and the
    bytes under each are that release's own. `legacy_paths` writes where one copy served every
    release that named it instead, which is what the last release published before this layout does
    so that installs made from it can still be updated.
    """
    if legacy_paths:
        return os.path.join(out_dir, component, version, arch)
    return os.path.join(out_dir, component, version, release, arch)


def stage(build_dir, out_dir, channel, release, versions, ui_tarball=None,
          source_commit=None, source_dirty=True, changelogs=None, legacy_paths=False):
    """Writes the release file, `latest`, and every component of the release. Returns the release file's content."""
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

    # Before anything is hashed or packed, because the release is the one thing a build cannot know
    # about itself while it is being built -- it is derived here, from what the channels publish.
    # build/ is stamped rather than a copy of it, so what is published and what ./launch.sh runs are
    # the same bytes, and a re-stage writes the new number over the old one.
    for component in sorted(_ARTIFACTS):
        stamp_path = _ARTIFACTS[component][2]
        if stamp_path is not None:
            stamp(os.path.join(build_dir, stamp_path), release)

    # Every component, every release. A component version's bytes used to be packed once and reused
    # by every release that named it; they cannot be now, because each release's copy carries that
    # release's number inside it and is a different file.
    components = {}
    for component, (filename, _, _) in _ARTIFACTS.items():
        version = versions[component]
        arches = {arch: None for arch in _ARCHES}

        dest_dir = component_dir(out_dir, component, version, release, "x86_64", legacy_paths)
        os.makedirs(dest_dir, exist_ok=True)
        dest = os.path.join(dest_dir, filename)
        if component == "compat":
            _pack_layer(build_dir, dest)
        elif component == "libtuxblox":
            shutil.copy2(ui_tarball, dest)
        else:
            shutil.copy2(os.path.join(build_dir, filename), dest)
        # Where these bytes are, relative to client/. The launcher and the installer build the
        # address themselves and do not read this; it is for a reader that cannot fall back from a
        # 404, such as the download page, which lists releases from both sides of the layout change
        # and has nothing else to tell them apart by.
        arches["x86_64"] = {"sha256": _digest(dest), "size": os.path.getsize(dest),
                            "path": os.path.relpath(dest, out_dir)}
        # Under the component version rather than beside the bytes: the account of what changed is
        # the same whichever architecture you downloaded and whichever release you took it in.
        changelog = (changelogs or {}).get(component)
        if changelog:
            shutil.copy2(changelog, os.path.join(out_dir, component, version, "changelogs.md"))

        components[component] = {"version": version, "arch": arches}

    # deploy.sh refuses a release whose source is not clean, equal to HEAD and pushed to every
    # remote, because a published binary whose source nobody can fetch is a licence violation.
    # Unknown reads as dirty so an unattributable build is refused rather than published.
    # When this release was published, which is the one date it has that never changes. UTC, so it
    # reads the same for whoever opens it, and written here rather than left to a file's mtime --
    # restoring a backup or re-syncing the tree would silently rewrite every release date.
    published_at = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    content = {"release": release, "channel": channel, "published": published_at,
               "components": components,
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
