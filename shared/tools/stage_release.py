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

from bump_check import components_touched, guard, scope_of_check
from component_versions import read_component_versions
from provenance import read_provenance, write_provenance
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


def build_source(root):
    """(commit, dirty) as recorded when the build STARTED, not as HEAD looks now. --stage-only
    republishes an older build/, so reading HEAD here would attribute a release to source that
    build never saw -- which is exactly what deploy.sh's licensing gate exists to catch."""
    commit, dirty, _ = read_provenance(os.path.join(root, "build", ".provenance"))
    return commit, dirty


def component_changelogs(root):
    """Each component's own changelogs.md, for the ones that keep one beside their sources.

    The file says what changed in the version being published and nothing else, so it is published as
    it stands. It is the repo owner's own writing, so nothing here ever edits it, and a component
    without one publishes no changelog rather than an invented one.
    """
    found = {}
    for component in ("launcher", "installer", "studio-mcp", "compat"):
        path = os.path.join(root, component, "changelogs.md")
        if os.path.isfile(path) and os.path.getsize(path) > 0:
            found[component] = path
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--channel", required=True)
    parser.add_argument("--seed", required=True,
                        help="the floor, MAJOR.MINOR.PATCH, from line 1 of the root VERSION file")
    parser.add_argument("--build", default="0",
                        help="the BUILD counter, from line 2; it is the THIRD field of a release")
    parser.add_argument("--release", default="", help="an explicit release, skipping resolution")
    parser.add_argument("--legacy-paths", action="store_true",
                        help="publish each component where one copy served every release that named "
                             "it, rather than under the release it is published in. For the last "
                             "release before the new layout, so installs made from it still update.")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    versions = read_component_versions(root)
    base_url = os.environ.get("TUXBLOX_SETUP_URL", DEFAULT_SETUP_URL)

    # An explicit release overrides only the NUMBER. What the channels already publish is still
    # fetched, because it is what says which component versions have moved since the last release --
    # the one thing the bump check cannot work out from this tree alone.
    try:
        resolved = resolve(versions, args.channel, http_fetch(base_url), args.seed,
                           int(args.build) if str(args.build).isdigit() else 0)
    except Unreachable as e:
        if not args.release:
            print("!! Cannot reach %s, so the release version cannot be resolved: %s" % (base_url, e),
                  file=sys.stderr)
            print("!! Set TUXBLOX_BUILD_VERSION to stage offline.", file=sys.stderr)
            return 1
        print(".. Cannot reach %s, so no component can be said to have been bumped or not." % base_url,
              file=sys.stderr)
        print(".. Everything is staged and nothing is checked against the last release.",
              file=sys.stderr)
        resolved = {"release": args.release, "published": {}, "base_commit": None}
    except NotFound as e:
        print("!! A channel points at a release whose file is missing: %s" % e, file=sys.stderr)
        return 1

    release = args.release or resolved["release"]
    base = resolved["base_commit"]

    # Which components really changed, measured from the commit the previous release was built from.
    scope, note = scope_of_check(base, git(root, "cat-file", "-t", base or "") == "commit")
    if scope == "measured":
        # -z and a NUL split, so a path containing a space is not two paths.
        changed = [p for p in git(root, "diff", "--name-only", "-z", "%s..HEAD" % base).split("\0") if p]
        touched = components_touched(changed)
        # A component whose version is not already published is one whose VERSION moved.
        moved = {c for c, v in versions.items() if (c, v) not in resolved["published"]}
    else:
        if note:
            print(".. %s" % note, file=sys.stderr)
        # Nothing to diff against, so nothing can be reported as a missed bump.
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

    # The build's own record, not HEAD: see build_source().
    commit, build_dirty = build_source(root)
    content = stage(os.path.join(root, "build"), os.path.join(root, "releases", "client"),
                    args.channel, release, versions,
                    source_commit=commit or None, source_dirty=build_dirty,
                    changelogs=component_changelogs(root), legacy_paths=args.legacy_paths)
    # Only the release line is added; the commit and dirty flag stay as the build wrote them.
    write_provenance(os.path.join(root, "build", ".provenance"), commit, build_dirty, release)

    print(":: Release %s (%s)%s" % (release, args.channel,
                                     " -- legacy paths" if args.legacy_paths else ""), file=sys.stderr)
    for component, entry in sorted(content["components"].items()):
        print("   %-12s %-8s published" % (component, entry["version"]), file=sys.stderr)
    print(release)
    return 0


if __name__ == "__main__":
    # Every other failure in a build reads as "!! <what went wrong>". A traceback at the end of a
    # multi-hour build buries the one line that says which file to fix.
    try:
        sys.exit(main())
    except (ValueError, RuntimeError) as error:
        print("!! %s" % error, file=sys.stderr)
        sys.exit(1)
