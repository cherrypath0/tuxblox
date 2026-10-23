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

"""Stages a release: resolves its version, checks the version bumps, and writes the client tree."""

import argparse
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request

from bump_check import components_touched, guard
from component_versions import read_component_versions
from provenance import write_provenance
from release_resolve import NotFound, Unreachable, resolve
from stage_tree import stage

DEFAULT_SETUP_URL = "https://setup.tuxblox.net"


def http_fetch(base_url):
    def fetch(path):
        url = "%s/%s" % (base_url.rstrip("/"), path)
        try:
            with urllib.request.urlopen(url, timeout=15) as response:
                return response.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            if e.code == 404:
                raise NotFound(url)
            raise Unreachable("%s: HTTP %d" % (url, e.code))
        except OSError as e:
            raise Unreachable("%s: %s" % (url, e))
    return fetch


def git(root, *args):
    done = subprocess.run(("git", "-C", root) + args, capture_output=True, text=True)
    return done.stdout.strip() if done.returncode == 0 else ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--channel", required=True)
    parser.add_argument("--seed", required=True, help="the release floor, from the root VERSION file")
    parser.add_argument("--release", default="", help="an explicit release, skipping resolution")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    versions = read_component_versions(root)
    base_url = os.environ.get("TUXBLOX_SETUP_URL", DEFAULT_SETUP_URL)

    # An explicit release overrides only the NUMBER. The reuse map is still fetched, because a
    # component version's bytes are published once and must never be packed again: tar records
    # mtimes, so repacking identical contents changes the sha256 and the server rejects it.
    try:
        resolved = resolve(versions, args.channel, http_fetch(base_url), args.seed)
    except Unreachable as e:
        if not args.release:
            print("!! Cannot reach %s, so the release version cannot be resolved: %s" % (base_url, e),
                  file=sys.stderr)
            print("!! Set TUXBLOX_BUILD_VERSION to stage offline.", file=sys.stderr)
            return 1
        print(".. Cannot reach %s, so nothing can be reused and every component is repacked." % base_url,
              file=sys.stderr)
        print(".. A repacked archive hashes differently, so this build may be rejected on deploy.",
              file=sys.stderr)
        resolved = {"release": args.release, "published": {}, "base_commit": None}
    except NotFound as e:
        print("!! A channel points at a release whose file is missing: %s" % e, file=sys.stderr)
        return 1

    release = args.release or resolved["release"]
    base = resolved["base_commit"]

    # Which components really changed, measured from the commit the previous release was built from.
    # With nothing published there is no earlier commit, so every component counts as new.
    if base and git(root, "cat-file", "-t", base) == "commit":
        changed = git(root, "diff", "--name-only", "%s..HEAD" % base).split()
        touched = components_touched(changed)
        # A component whose version is not already published is one whose VERSION moved.
        moved = {c for c, v in versions.items() if (c, v) not in resolved["published"]}
    else:
        touched = set(versions)
        moved = set(versions)

    dirty = bool(git(root, "status", "--porcelain"))
    branch = git(root, "branch", "--show-current")
    verdict, lines = guard(touched, moved, dirty, branch)
    for line in lines:
        print("%s %s" % ("!!" if verdict == "refuse" else "..", line), file=sys.stderr)
    if verdict == "refuse":
        if os.environ.get("TUXBLOX_SKIP_BUMP_CHECK") == "1":
            print(".. TUXBLOX_SKIP_BUMP_CHECK=1, continuing anyway", file=sys.stderr)
        else:
            print("!! Bump the VERSION files above, or set TUXBLOX_SKIP_BUMP_CHECK=1.", file=sys.stderr)
            return 1

    commit = git(root, "rev-parse", "HEAD")
    content = stage(os.path.join(root, "build"), os.path.join(root, "releases", "client"),
                    args.channel, release, versions, resolved["published"],
                    source_commit=commit or None, source_dirty=dirty)
    write_provenance(os.path.join(root, "build", ".provenance"), commit, dirty, release)

    reused = [c for c, v in versions.items() if (c, v) in resolved["published"]]
    print(":: Release %s (%s)" % (release, args.channel), file=sys.stderr)
    for component, entry in sorted(content["components"].items()):
        print("   %-12s %-8s %s" % (component, entry["version"],
                                     "reused" if component in reused else "published"), file=sys.stderr)
    print(release)
    return 0


if __name__ == "__main__":
    sys.exit(main())
