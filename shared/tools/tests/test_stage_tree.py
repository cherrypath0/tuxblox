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

import hashlib
import json
import datetime
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from stage_tree import stage
from stamp_release import FIELD_SIZE, MARKER
from stamp_release import read as read_stamp

VERSIONS = {"launcher": "3.5.0", "compat": "3.3.0", "installer": "3.1.0",
            "libtuxblox": "3.1.0", "studio-mcp": "2.0.1"}
COMMIT = "0123456789abcdef0123456789abcdef01234567"


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class StageTree(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="tuxblox-stage-")
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.build = os.path.join(self.tmp, "build")
        self.out = os.path.join(self.tmp, "out")
        self.dist = os.path.join(self.tmp, "dist")
        os.makedirs(self.dist)

        self.write_binary("compat/main", "layer\n")
        self.write("compat/files/share/thing", "data\n")
        self.write_binary("TuxBloxLauncher", "launcher\n")
        self.write_binary("TuxBloxInstaller", "installer\n")
        self.write_binary("studio-mcp", "mcp\n")
        self.write("libtuxblox/lib/x86_64-linux-gnu/libgtk-4.so", "so\n")
        self.write("libtuxblox/share/icons/TuxBlox/index.theme", "theme\n")
        self.write("libtuxblox/LIBADWAITA_COMMIT", COMMIT + "\n")

        # The real stack tarball, which stage() copies rather than repacks.
        stagedir = os.path.join(self.tmp, "stack")
        os.makedirs(os.path.join(stagedir, "lib"))
        with open(os.path.join(stagedir, "LIBADWAITA_COMMIT"), "w") as f:
            f.write(COMMIT + "\n")
        with open(os.path.join(stagedir, "VERSION"), "w") as f:
            f.write(VERSIONS["libtuxblox"] + "\n")
        self.ui_tarball = os.path.join(self.dist, "ui-stack-3-x86_64.tar.zst")
        subprocess.run("tar -C %s -cf - lib LIBADWAITA_COMMIT VERSION | zstd -1 -q -o %s -f"
                       % (stagedir, self.ui_tarball), shell=True, check=True)

    def write(self, rel, text):
        path = os.path.join(self.build, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)

    # A stand-in for a TuxBlox binary: text, carrying the field every one of them reserves for the
    # release, exactly as the compiler leaves it -- the marker and then nothing.
    def write_binary(self, rel, text):
        path = os.path.join(self.build, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(text.encode() + MARKER + b"\0" * (FIELD_SIZE - len(MARKER)))

    def stage(self, channel="stable", release="3.6.0.0", versions=None, legacy_paths=False):
        return stage(self.build, self.out, channel, release, versions or VERSIONS,
                     ui_tarball=self.ui_tarball, legacy_paths=legacy_paths)

    def path(self, *parts):
        return os.path.join(self.out, *parts)

    # Where this release's copy of a component is written.
    def component(self, name, version, filename, release="3.6.0.0"):
        return self.path(name, version, release, "x86_64", filename)

    def test_writes_the_release_file_and_latest(self):
        out = self.stage()
        self.assertEqual(out["release"], "3.6.0.0")
        self.assertEqual(out["channel"], "stable")
        with open(self.path("releases", "stable", "latest")) as f:
            self.assertEqual(f.read().strip(), "3.6.0.0")
        self.assertEqual(set(out["components"]), set(VERSIONS))
        with open(self.path("releases", "stable", "3.6.0.0.json")) as f:
            self.assertEqual(json.load(f), out)

    def test_unpublished_arch_is_null_not_a_placeholder(self):
        out = self.stage()
        self.assertIsNone(out["components"]["launcher"]["arch"]["arm64"])
        self.assertGreater(out["components"]["launcher"]["arch"]["x86_64"]["size"], 0)

    def test_size_not_bytes(self):
        entry = self.stage()["components"]["compat"]["arch"]["x86_64"]
        self.assertIn("size", entry)
        self.assertNotIn("bytes", entry)

    def test_component_paths_carry_no_channel(self):
        self.stage(channel="canary")
        self.assertTrue(os.path.exists(self.component("compat", "3.3.0", "compat.tar.zst")))
        self.assertFalse(os.path.exists(self.path("canary")))

    def test_hash_describes_the_downloaded_file(self):
        out = self.stage()
        tarball = self.component("compat", "3.3.0", "compat.tar.zst")
        entry = out["components"]["compat"]["arch"]["x86_64"]
        self.assertEqual(entry["sha256"], sha256_of(tarball))
        self.assertEqual(entry["size"], os.path.getsize(tarball))

    # The layer's archive has its contents at the root, with no wrapper directory, or the
    # installer would extract it one level too deep.
    def test_the_layer_archive_has_no_wrapper_directory(self):
        self.stage()
        names = subprocess.run(["tar", "--zstd", "-tf",
                                self.component("compat", "3.3.0", "compat.tar.zst")],
                               capture_output=True, text=True, check=True).stdout.split()
        self.assertIn("./main", names)
        self.assertNotIn("./compat/main", names)

    # The stack tarball is copied, never repacked, so its checksum does not change on a re-stage.
    def test_the_stack_tarball_is_copied_byte_for_byte(self):
        out = self.stage()
        copied = self.component("libtuxblox", "3.1.0", "libtuxblox.tar.zst")
        self.assertEqual(sha256_of(copied), sha256_of(self.ui_tarball))
        self.assertEqual(out["components"]["libtuxblox"]["arch"]["x86_64"]["sha256"], sha256_of(self.ui_tarball))

    def test_missing_build_artifact_fails_naming_it(self):
        os.remove(os.path.join(self.build, "compat", "main"))
        with self.assertRaisesRegex(RuntimeError, "compat"):
            self.stage()

    def test_a_stack_built_from_another_libadwaita_is_refused(self):
        self.write("libtuxblox/LIBADWAITA_COMMIT", "f" * 40 + "\n")
        with self.assertRaisesRegex(RuntimeError, "LIBADWAITA_COMMIT"):
            self.stage()

    # The virtual drive can hold a logged-in Roblox session, so it is never copied.
    def test_the_runtime_and_artifacts_directories_are_never_copied(self):
        self.write("runtime/pfx/drive_c/secret", "cookie\n")
        self.write(".artifacts/proton/scratch", "junk\n")
        self.stage()
        for bad in ("runtime", ".artifacts"):
            self.assertFalse(any(bad in root for root, _, _ in os.walk(self.out)))


class Changelogs(StageTree):
    def test_the_release_file_records_when_it_was_published(self):
        # The one date a release has that never changes. Written here rather than read off the
        # filesystem later, because restoring a backup or re-syncing the tree would silently
        # rewrite every release date if it came from an mtime.
        published = self.stage()["published"]
        parsed = datetime.datetime.strptime(published, "%Y-%m-%dT%H:%M:%SZ")
        self.assertLess(abs((datetime.datetime.utcnow() - parsed).total_seconds()), 300)
        self.assertTrue(published.endswith("Z"), "the timestamp has to say it is UTC")

    def test_the_release_file_carries_no_changelog_kind(self):
        # It only ever had one value, so it told a reader nothing -- and it claimed per-component
        # changelogs that do not exist yet. A site asks for the release-level .md and merges the
        # per-component ones when there is none.
        self.assertNotIn("changelog", self.stage())

    def test_a_given_changelog_is_written_beside_its_component(self):
        src = os.path.join(self.tmp, "launcher-changelog.md")
        with open(src, "w") as f:
            f.write("- Fixed a thing\n")
        stage(self.build, self.out, "stable", "3.6.0.0", VERSIONS,
              ui_tarball=self.ui_tarball, changelogs={"launcher": src})
        with open(self.path("launcher", "3.5.0", "changelogs.md")) as f:
            self.assertEqual(f.read(), "- Fixed a thing\n")

    def test_a_component_with_no_changelog_gets_no_file(self):
        self.stage()
        self.assertFalse(os.path.exists(self.path("compat", "3.3.0", "changelogs.md")))

    # It sits under the component version, not under the release, because what changed in a
    # component version is the same account however many releases publish a copy of it.
    def test_the_changelog_sits_above_every_release_that_carries_the_component(self):
        src = os.path.join(self.tmp, "compat-changelog.md")
        with open(src, "w") as f:
            f.write("- Layer change\n")
        for release in ("3.6.0.0", "3.6.0.1"):
            stage(self.build, self.out, "stable", release, VERSIONS,
                  ui_tarball=self.ui_tarball, changelogs={"compat": src})
        with open(self.path("compat", "3.3.0", "changelogs.md")) as f:
            self.assertEqual(f.read(), "- Layer change\n")
        for release in ("3.6.0.0", "3.6.0.1"):
            self.assertTrue(os.path.exists(
                self.component("compat", "3.3.0", "compat.tar.zst", release)))


class StackVersionMatch(StageTree):
    """The stack tarball is COPIED, not repacked, so its contents are whatever the last hour-long
    stack build produced. If its own VERSION disagrees with LIBTUXBLOX_VERSION the copy is published
    under a version it does not contain -- and component bytes are immutable at a published version,
    so that version is then unfixable forever."""

    def repack(self, files):
        stagedir = os.path.join(self.tmp, "stack2")
        os.makedirs(os.path.join(stagedir, "lib"), exist_ok=True)
        for name, text in files.items():
            with open(os.path.join(stagedir, name), "w") as f:
                f.write(text)
        subprocess.run("tar -C %s -cf - lib %s | zstd -1 -q -o %s -f"
                       % (stagedir, " ".join(files), self.ui_tarball),
                       shell=True, check=True)

    def test_a_tarball_whose_version_disagrees_is_refused(self):
        self.repack({"LIBADWAITA_COMMIT": COMMIT + "\n", "VERSION": "3.0.0\n"})
        with self.assertRaisesRegex(RuntimeError, "3.0.0"):
            self.stage()

    # The state the tree is actually in right now: the stack predates the VERSION file.
    def test_a_tarball_with_no_version_at_all_is_refused(self):
        self.repack({"LIBADWAITA_COMMIT": COMMIT + "\n"})
        with self.assertRaisesRegex(RuntimeError, "VERSION"):
            self.stage()

    def test_a_matching_tarball_is_accepted(self):
        self.repack({"LIBADWAITA_COMMIT": COMMIT + "\n", "VERSION": "3.1.0\n"})
        self.assertEqual(self.stage()["components"]["libtuxblox"]["version"], "3.1.0")


class Provenance(StageTree):
    """deploy.sh's licensing gate refuses a release whose source is not clean, equal to HEAD and
    pushed, so the release file has to carry the commit it was built from. The bump guard needs it
    too, to know what "changed since this component was last published" is measured against."""

    def test_the_release_file_records_its_source(self):
        out = stage(self.build, self.out, "stable", "3.6.0.0", VERSIONS,
                    ui_tarball=self.ui_tarball, source_commit="abc123", source_dirty=False)
        self.assertEqual(out["source_commit"], "abc123")
        self.assertIs(out["source_dirty"], False)

    # Unknown source must read as dirty, so the gate refuses it rather than publishing an
    # unattributable build.
    def test_an_unknown_source_is_recorded_as_dirty(self):
        out = self.stage()
        self.assertIsNone(out["source_commit"])
        self.assertIs(out["source_dirty"], True)


class EveryReleasePublishesEverything(StageTree):
    """A component version's bytes used to be packed exactly once and reused by every release that
    named it. They cannot be now: each release's copy carries that release's number inside it, so no
    two releases' copies are the same file and each release publishes its own."""

    def test_a_component_whose_version_did_not_move_is_published_again(self):
        self.stage(release="3.6.0.0")
        self.stage(release="3.6.0.1")
        for release in ("3.6.0.0", "3.6.0.1"):
            self.assertTrue(os.path.exists(
                self.component("compat", "3.3.0", "compat.tar.zst", release)))

    def test_one_component_version_has_different_bytes_in_each_release(self):
        first = self.stage(release="3.6.0.0")["components"]["launcher"]["arch"]["x86_64"]
        second = self.stage(release="3.6.0.1")["components"]["launcher"]["arch"]["x86_64"]
        self.assertNotEqual(first["sha256"], second["sha256"])
        self.assertEqual(first["size"], second["size"])   # a fixed-size field, so the size holds

    def test_the_release_is_written_into_every_binary(self):
        self.stage(release="3.6.0.0")
        for name, version, filename in (("launcher", "3.5.0", "TuxBloxLauncher"),
                                         ("installer", "3.1.0", "TuxBloxInstaller"),
                                         ("studio-mcp", "2.0.1", "studio-mcp")):
            self.assertEqual(read_stamp(self.component(name, version, filename)), "3.6.0.0")

    # The build's own copy, not just the published one: ./launch.sh runs what is in build/, and a
    # re-stage under another number has to leave nothing of the old one behind.
    def test_the_build_is_stamped_in_place(self):
        self.stage(release="3.6.0.0")
        self.assertEqual(read_stamp(os.path.join(self.build, "TuxBloxLauncher")), "3.6.0.0")
        self.stage(release="3.10.11.0")
        self.assertEqual(read_stamp(os.path.join(self.build, "TuxBloxLauncher")), "3.10.11.0")

    def test_the_layer_is_packed_with_the_release_inside_it(self):
        self.stage(release="3.6.0.0")
        self.assertEqual(read_stamp(os.path.join(self.build, "compat", "main")), "3.6.0.0")
        packed = subprocess.run(["tar", "--zstd", "-xOf",
                                 self.component("compat", "3.3.0", "compat.tar.zst"), "./main"],
                                capture_output=True, check=True).stdout
        self.assertIn(MARKER + b"3.6.0.0\0", packed)

    # The one component with no binary to write a number into. It is still published under every
    # release, because a client builds its address out of the release it is installing.
    def test_the_stack_carries_no_release_and_is_published_under_each_one(self):
        self.stage(release="3.6.0.0")
        self.stage(release="3.6.0.1")
        for release in ("3.6.0.0", "3.6.0.1"):
            copied = self.component("libtuxblox", "3.1.0", "libtuxblox.tar.zst", release)
            self.assertEqual(sha256_of(copied), sha256_of(self.ui_tarball))
            self.assertIsNone(read_stamp(copied))

    def test_a_binary_with_no_field_is_refused_naming_it(self):
        self.write("TuxBloxLauncher", "a build from before the field existed\n")
        with self.assertRaisesRegex(RuntimeError, "TuxBloxLauncher"):
            self.stage()

    # Two copies of the field would mean stamping one and reading the other.
    def test_a_binary_carrying_the_field_twice_is_refused(self):
        path = os.path.join(self.build, "studio-mcp")
        with open(path, "rb") as f:
            doubled = f.read()
        with open(path, "wb") as f:
            f.write(doubled + doubled)
        with self.assertRaisesRegex(RuntimeError, "more than once"):
            self.stage()


class LegacyPaths(StageTree):
    """The last release published before each one carried its own copy of every component. Installs
    made from it build an address with no release in it, so that one release is published where they
    will look for it -- and every release after it is not."""

    def test_components_are_written_where_one_copy_served_every_release(self):
        self.stage(legacy_paths=True)
        self.assertTrue(os.path.exists(self.path("launcher", "3.5.0", "x86_64", "TuxBloxLauncher")))
        self.assertFalse(os.path.exists(self.component("launcher", "3.5.0", "TuxBloxLauncher")))

    def test_the_release_is_still_written_into_them(self):
        self.stage(legacy_paths=True)
        self.assertEqual(read_stamp(self.path("launcher", "3.5.0", "x86_64", "TuxBloxLauncher")),
                         "3.6.0.0")

    # Only where the bytes are differs, and the file says which.
    def test_the_release_file_says_the_same_thing_either_way(self):
        legacy = self.stage(legacy_paths=True)
        current = self.stage()
        self.assertEqual(legacy["components"]["launcher"]["arch"]["x86_64"]["path"],
                         "launcher/3.5.0/x86_64/TuxBloxLauncher")
        self.assertEqual(current["components"]["launcher"]["arch"]["x86_64"]["path"],
                         "launcher/3.5.0/3.6.0.0/x86_64/TuxBloxLauncher")
        for content in (legacy, current):
            del content["published"]
            for entry in content["components"].values():
                del entry["arch"]["x86_64"]["path"]
        self.assertEqual(legacy, current)


class StackTarballSelection(StageTree):
    """With no explicit tarball, stage() must find it by UI_STACK_VERSION rather than by a fixed name."""

    def test_selects_the_tarball_named_by_the_stack_version(self):
        root = os.path.join(self.tmp, "repo")
        shutil.copytree(self.build, os.path.join(root, "build"))
        os.makedirs(os.path.join(root, "shared", "tools"))
        os.makedirs(os.path.join(root, "shared", "ui", "dist"))
        with open(os.path.join(root, "shared", "tools", "ui-versions.env"), "w") as f:
            f.write("UI_STACK_VERSION=7\nLIBTUXBLOX_VERSION=3.1.0\n")
        # Only the version-7 tarball is the one it may pick; a stale 3 sits beside it.
        shutil.copy2(self.ui_tarball, os.path.join(root, "shared", "ui", "dist", "ui-stack-7-x86_64.tar.zst"))
        with open(os.path.join(root, "shared", "ui", "dist", "ui-stack-3-x86_64.tar.zst"), "w") as f:
            f.write("stale, and not a valid archive\n")
        out = os.path.join(self.tmp, "out7")
        content = stage(os.path.join(root, "build"), out, "stable", "3.6.0.0", VERSIONS)
        copied = os.path.join(out, "libtuxblox", "3.1.0", "3.6.0.0", "x86_64", "libtuxblox.tar.zst")
        self.assertEqual(sha256_of(copied), sha256_of(self.ui_tarball))
        self.assertEqual(content["components"]["libtuxblox"]["arch"]["x86_64"]["sha256"],
                         sha256_of(self.ui_tarball))


if __name__ == "__main__":
    unittest.main()
