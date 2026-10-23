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
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from stage_tree import stage

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

        self.write("compat/main", "layer\n")
        self.write("compat/files/share/thing", "data\n")
        self.write("TuxBloxLauncher", "launcher\n")
        self.write("TuxBloxInstaller", "installer\n")
        self.write("studio-mcp", "mcp\n")
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

    def stage(self, channel="stable", release="3.6.0.0", versions=None, published=None):
        return stage(self.build, self.out, channel, release,
                     versions or VERSIONS, published or {}, ui_tarball=self.ui_tarball)

    def path(self, *parts):
        return os.path.join(self.out, *parts)

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
        self.assertTrue(os.path.exists(self.path("components", "compat", "3.3.0", "x86_64", "compat.tar.zst")))
        self.assertFalse(os.path.exists(self.path("components", "canary")))

    def test_hash_describes_the_downloaded_file(self):
        out = self.stage()
        tarball = self.path("components", "compat", "3.3.0", "x86_64", "compat.tar.zst")
        entry = out["components"]["compat"]["arch"]["x86_64"]
        self.assertEqual(entry["sha256"], sha256_of(tarball))
        self.assertEqual(entry["size"], os.path.getsize(tarball))

    # The layer's archive has its contents at the root, with no wrapper directory, or the
    # installer would extract it one level too deep.
    def test_the_layer_archive_has_no_wrapper_directory(self):
        self.stage()
        names = subprocess.run(["tar", "--zstd", "-tf",
                                self.path("components", "compat", "3.3.0", "x86_64", "compat.tar.zst")],
                               capture_output=True, text=True, check=True).stdout.split()
        self.assertIn("./main", names)
        self.assertNotIn("./compat/main", names)

    # The stack tarball is copied, never repacked, so its checksum does not change on a re-stage.
    def test_the_stack_tarball_is_copied_byte_for_byte(self):
        out = self.stage()
        copied = self.path("components", "libtuxblox", "3.1.0", "x86_64", "libtuxblox.tar.zst")
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


class Provenance(StageTree):
    """deploy.sh's licensing gate refuses a release whose source is not clean, equal to HEAD and
    pushed, so the release file has to carry the commit it was built from. The bump guard needs it
    too, to know what "changed since this component was last published" is measured against."""

    def test_the_release_file_records_its_source(self):
        out = stage(self.build, self.out, "stable", "3.6.0.0", VERSIONS, {},
                    ui_tarball=self.ui_tarball, source_commit="abc123", source_dirty=False)
        self.assertEqual(out["source_commit"], "abc123")
        self.assertIs(out["source_dirty"], False)

    # Unknown source must read as dirty, so the gate refuses it rather than publishing an
    # unattributable build.
    def test_an_unknown_source_is_recorded_as_dirty(self):
        out = self.stage()
        self.assertIsNone(out["source_commit"])
        self.assertIs(out["source_dirty"], True)


class AlreadyPublished(StageTree):
    """A component version's bytes are packed exactly once, ever: tar records mtimes, so a repack
    of identical contents hashes differently and would defeat the client's "skip what I have"."""

    PUBLISHED = {("compat", "3.3.0"): {"sha256": "de" * 32, "size": 349829060}}

    def test_already_published_component_is_not_repacked(self):
        out = self.stage(release="3.6.0.1", published=self.PUBLISHED)
        entry = out["components"]["compat"]["arch"]["x86_64"]
        self.assertEqual(entry["sha256"], "de" * 32)
        self.assertEqual(entry["size"], 349829060)
        self.assertFalse(os.path.exists(
            self.path("components", "compat", "3.3.0", "x86_64", "compat.tar.zst")))

    def test_a_changed_component_is_still_packed(self):
        versions = dict(VERSIONS, launcher="3.6.0")
        self.stage(release="3.6.0.1", versions=versions, published=self.PUBLISHED)
        self.assertTrue(os.path.exists(
            self.path("components", "launcher", "3.6.0", "x86_64", "TuxBloxLauncher")))

    # A published entry is matched on version too, not just component name.
    def test_a_different_version_of_a_published_component_is_packed(self):
        versions = dict(VERSIONS, compat="3.4.0")
        self.stage(release="3.6.0.1", versions=versions, published=self.PUBLISHED)
        self.assertTrue(os.path.exists(
            self.path("components", "compat", "3.4.0", "x86_64", "compat.tar.zst")))

    # Skipping must not leave an empty directory behind that a sync would publish as a 0-byte artifact.
    def test_no_empty_directory_is_left_for_a_skipped_component(self):
        self.stage(release="3.6.0.1", published=self.PUBLISHED)
        self.assertFalse(os.path.isdir(self.path("components", "compat")))


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
        content = stage(os.path.join(root, "build"), out, "stable", "3.6.0.0", VERSIONS, {})
        copied = os.path.join(out, "components", "libtuxblox", "3.1.0", "x86_64", "libtuxblox.tar.zst")
        self.assertEqual(sha256_of(copied), sha256_of(self.ui_tarball))
        self.assertEqual(content["components"]["libtuxblox"]["arch"]["x86_64"]["sha256"],
                         sha256_of(self.ui_tarball))


if __name__ == "__main__":
    unittest.main()
