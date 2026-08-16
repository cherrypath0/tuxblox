#!/bin/bash
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

# Maintainer helper (not shipped in the package): refresh the PKGBUILD to
# the latest published stable release. Reads /v2/latest.json for the
# version and the per-version manifest for the installer artifact's
# sha256 -- the same values the installer itself trusts -- then rewrites
# pkgver / pkgrel / the installer entry of sha256sums and regenerates
# .SRCINFO. Run this after package.sh has published a release, then
# commit PKGBUILD + .SRCINFO to the AUR repo (see README.md).
set -euo pipefail
cd "$(dirname "$0")"

base="https://setup.tuxblox.net"

ver=$(curl -fsSL "$base/v2/latest.json" | python3 -c '
import json, sys
v = json.load(sys.stdin)["channels"]["stable"]
assert v, "no stable release published yet"
print(v)')

sha=$(curl -fsSL "$base/v1/stable/$ver/manifest.json" | python3 -c '
import json, sys
print(json.load(sys.stdin)["artifacts"]["installer"]["sha256"])')

python3 - "$ver" "$sha" <<'EOF'
import re, sys
ver, sha = sys.argv[1], sys.argv[2]
src = open("PKGBUILD").read()
src, n1 = re.subn(r"^pkgver=.*$", f"pkgver={ver}", src, count=1, flags=re.M)
src, n2 = re.subn(r"^pkgrel=.*$", "pkgrel=1", src, count=1, flags=re.M)
src, n3 = re.subn(r"^(sha256sums=\(')[0-9a-f]{64}(')", rf"\g<1>{sha}\g<2>", src,
                  count=1, flags=re.M)
assert n1 == n2 == n3 == 1, f"PKGBUILD rewrite matched ({n1}, {n2}, {n3}) times, expected 1 each"
open("PKGBUILD", "w").write(src)
EOF

makepkg --printsrcinfo > .SRCINFO

echo ":: PKGBUILD now at ${ver} (installer sha256 ${sha})."
echo ":: Review with 'git diff', test-build with 'makepkg -f', then copy"
echo ":: the changed files into the AUR checkout, commit, and push."
