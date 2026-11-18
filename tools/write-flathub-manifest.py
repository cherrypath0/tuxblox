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

"""Write the Flathub copy of the manifest: the same build, pinned to a stable release's source tarball."""

import argparse
import hashlib
import os
import re
import sys
import urllib.request

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
PACKAGING = os.path.join(HERE, "..", "packaging", "flatpak")
TARBALL = "https://github.com/tuxblox/tuxblox/archive/refs/tags/%s.tar.gz"
ENV_ANCHOR = "        TUXBLOX_DONT_INCREMENT_BUILD: '1'\n"

SOURCES = """    sources:
      - type: archive
        url: %(tarball)s
        sha256: '%(sha256)s'
"""


def hashed(url):
    request = urllib.request.Request(url, headers={"User-Agent": "tuxblox-manifest-generator"})
    with urllib.request.urlopen(request) as response:
        digest = hashlib.sha256()
        for block in iter(lambda: response.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--sha256", help="the tarball's hash, instead of downloading it to find out")
    parser.add_argument("--manifest", default=os.path.join(PACKAGING, "net.tuxblox.TuxBlox.yml"))
    parser.add_argument("--metainfo", default=os.path.join(PACKAGING, "net.tuxblox.TuxBlox.metainfo.xml"))
    args = parser.parse_args()

    with open(args.metainfo, encoding="utf-8") as handle:
        if '<release version="%s"' % args.version not in handle.read():
            sys.exit("%s has no entry in %s: write its notes before pinning a manifest to it"
                     % (args.version, os.path.basename(args.metainfo)))

    with open(args.manifest, encoding="utf-8") as handle:
        text = handle.read()

    if ENV_ANCHOR not in text:
        sys.exit("%s no longer sets TUXBLOX_DONT_INCREMENT_BUILD, so there is nowhere to put the release"
                 % args.manifest)
    text = re.sub(r"\n{3,}", "\n\n", text)
    text = text.replace(ENV_ANCHOR, ENV_ANCHOR
                        + "        TUXBLOX_RELEASE: '%s'\n" % args.version
                        + "        TUXBLOX_CHANNEL: stable\n")

    marker = "\n    sources:\n"
    if marker not in text:
        sys.exit("%s has no sources block to replace" % args.manifest)
    tarball = TARBALL % args.version
    text = text[:text.index(marker) + 1] + SOURCES % {
        "tarball": tarball,
        "sha256": args.sha256 or hashed(tarball),
    }

    # A key lost to the surgery above would be silent
    written = yaml.safe_load(text)
    original = yaml.safe_load(open(args.manifest, encoding="utf-8"))
    if set(written) != set(original) or set(written["modules"][0]) != set(original["modules"][0]):
        sys.exit("the generated manifest lost a key; the in-tree manifest's shape must have changed")

    with open(args.out, "w", encoding="utf-8") as handle:
        handle.write(text)


if __name__ == "__main__":
    main()
