#!/usr/bin/env bash
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

# Downloads a pinned upstream Rosemary release and puts the executable at
# <output>/rosemary. The release is checked against a hash kept here, so a
# changed or truncated download cannot reach the launcher.
#
# Usage: fetch-rosemary.sh <output-dir>
set -euo pipefail

Version="0.5.0"
Asset="Rosemary-${Version}-x86_64.AppImage"
Sha256="04fbdae4d1052e2a65fa9442ded5117542da48a6022c36dd5c65e459c3adf108"
Url="https://github.com/microlift/rosemary/releases/download/v${Version}/${Asset}"

Out="${1:?usage: fetch-rosemary.sh <output-dir>}"
mkdir -p "$Out"

Work="$(mktemp -d)"
trap 'rm -rf "$Work"' EXIT

curl -fsSL --retry 3 -o "$Work/$Asset" "$Url"
printf '%s  %s\n' "$Sha256" "$Work/$Asset" | sha256sum -c -

cd "$Work"
chmod +x "$Asset"
"./$Asset" --appimage-extract >/dev/null

Binary="$(find squashfs-root -type f -name rosemary -perm -u+x -print -quit)"
if [ -z "$Binary" ]; then
    echo "fetch-rosemary: no rosemary executable in the release AppImage" >&2
    exit 1
fi
install -m 755 "$Binary" "$Out/rosemary"
echo "fetch-rosemary: installed $Out/rosemary"
