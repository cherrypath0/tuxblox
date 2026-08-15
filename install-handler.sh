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

set -e

repoDir="$(cd "$(dirname "$0")" && pwd)"
appsDir="$HOME/.local/share/applications"
playerDesktop="$appsDir/tuxblox-player.desktop"
studioDesktop="$appsDir/tuxblox-studio.desktop"

if [ "$1" = "--uninstall" ]; then
    rm -f "$playerDesktop" "$studioDesktop"
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$appsDir" >/dev/null 2>&1
    echo "Removed TuxBlox protocol handlers."
    exit 0
fi

if ! command -v xdg-mime >/dev/null 2>&1; then
    echo "xdg-mime is required to register URL handlers but was not found." >&2
    echo "Install your distro's xdg-utils package and re-run this script." >&2
    exit 1
fi

mkdir -p "$appsDir"

cat > "$playerDesktop" <<EOF
[Desktop Entry]
Type=Application
Name=TuxBlox Player Handler (Development)
Exec=$repoDir/launch.sh player %u
NoDisplay=true
Terminal=false
MimeType=x-scheme-handler/roblox-player;
EOF

cat > "$studioDesktop" <<EOF
[Desktop Entry]
Type=Application
Name=TuxBlox Studio Handler (Development)
Exec=$repoDir/launch.sh studio %u
NoDisplay=true
Terminal=false
MimeType=x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;
EOF

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$appsDir" >/dev/null 2>&1

xdg-mime default tuxblox-player.desktop x-scheme-handler/roblox-player
xdg-mime default tuxblox-studio.desktop x-scheme-handler/roblox-studio
xdg-mime default tuxblox-studio.desktop x-scheme-handler/roblox-studio-auth

echo "Registered this repo checkout (via launch.sh, prefix: $repoDir/runtime) as the"
echo "handler for roblox-player:, roblox-studio:, and roblox-studio-auth: links."
echo "An installed TuxBloxLauncher will no longer revert this on its own -- it only"
echo "reclaims a scheme if the default isn't one of these two dev handlers."
echo "Run '$0 --uninstall' to remove."
