#!/usr/bin/env python3
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

"""Refuse an archive that carries who built it or when.

A member's mtime and owner come from the build machine unless tar is told otherwise, and
two builds of one commit then differ in bytes nobody meant to publish.

Without --epoch the test is that every member shares one mtime, whatever it is, because an
archive is built once and checked long after, by which time the commit has moved on. Pass
--epoch only where the archive was just written and the value is known.
"""

import argparse
import contextlib
import subprocess
import sys
import tarfile

REPORT_LIMIT = 5


@contextlib.contextmanager
def opened(path):
    """tarfile only learned zstd in Python 3.14, and the build containers are older than that."""
    if not path.endswith((".zst", ".zstd")):
        with tarfile.open(path) as archive:
            yield archive
        return

    unzstd = subprocess.Popen(["zstd", "-dc", path], stdout=subprocess.PIPE)
    try:
        with tarfile.open(mode="r|", fileobj=unzstd.stdout) as archive:
            yield archive
    finally:
        # Drained before closing, because a streaming tar stops at the end-of-archive blocks and
        # leaves the padding behind it unread. Closing the pipe there hands zstd a write it cannot
        # complete, which it reports as failure, and a perfectly good archive then reads as corrupt.
        # It is only ever the trailing padding, since every caller here reads to the end.
        try:
            while unzstd.stdout.read(1 << 16):
                pass
        except OSError:
            pass
        unzstd.stdout.close()
        if unzstd.wait() != 0:
            raise OSError("zstd could not decompress %s" % path)


def offenders(path, epoch):
    """Every member whose metadata would differ between two builds, and the epoch they should share."""
    found = []
    total = 0
    with opened(path) as archive:
        for member in archive:
            total += 1
            if epoch is None:
                epoch = member.mtime
            if member.mtime != epoch:
                found.append("%s: mtime %d, expected %d" % (member.name, member.mtime, epoch))
            elif member.uid or member.gid:
                found.append("%s: owner %d:%d, expected 0:0" % (member.name, member.uid, member.gid))
    return found, total, epoch


def main():
    parser = argparse.ArgumentParser(description="Check a shipped archive is reproducible.")
    parser.add_argument("--epoch", type=int, help="the SOURCE_DATE_EPOCH the build stamped, if known")
    parser.add_argument("archive", nargs="+")
    args = parser.parse_args()

    failures = 0
    for path in args.archive:
        try:
            found, total, epoch = offenders(path, args.epoch)
        except (OSError, tarfile.TarError) as error:
            print("!! %s: cannot read: %s" % (path, error), file=sys.stderr)
            failures += 1
            continue

        if not total:
            print("!! %s: empty archive" % path, file=sys.stderr)
            failures += 1
            continue

        if not found:
            print(":: %s: %d members, all stamped %d" % (path, total, epoch))
            continue

        failures += 1
        print("!! %s: %d of %d members carry build metadata" % (path, len(found), total), file=sys.stderr)
        for line in found[:REPORT_LIMIT]:
            print("   %s" % line, file=sys.stderr)
        if len(found) > REPORT_LIMIT:
            print("   ... and %d more" % (len(found) - REPORT_LIMIT), file=sys.stderr)
        print("   the tar that wrote it needs --sort=name --mtime=@%d --owner=0 --group=0 --numeric-owner"
              % epoch, file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
