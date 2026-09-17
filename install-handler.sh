#!/usr/bin/env bash

# NOTE:
# This script only installs development URL handlers, and should only be used for testing purposes.
# This script is also very experimental and might break your URL handlers when you build again.
# TuxBlox's launcher will also no longer revert these development URL handlers.

set -e

repoDir="$(cd "$(dirname "$0")" && pwd)"
appsDir="$HOME/.local/share/applications"
playerDesktop="$appsDir/tuxblox-player-dev.desktop"
staleRobloxDesktop="$appsDir/tuxblox-roblox-dev.desktop"
studioDesktop="$appsDir/tuxblox-studio-dev.desktop"

if [ "$1" = "--uninstall" ]; then
    rm -f "$staleRobloxDesktop" "$playerDesktop" "$studioDesktop"
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
rm -f "$staleRobloxDesktop"

cat > "$playerDesktop" <<EOF
[Desktop Entry]
Type=Application
Name=TuxBlox Player (Development)
Exec=$repoDir/launch.sh player %u
NoDisplay=true
Terminal=false
MimeType=x-scheme-handler/roblox;x-scheme-handler/roblox-player;
EOF

cat > "$studioDesktop" <<EOF
[Desktop Entry]
Type=Application
Name=TuxBlox Studio (Development)
Exec=$repoDir/launch.sh studio %u
NoDisplay=true
Terminal=false
MimeType=x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;
EOF

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$appsDir" >/dev/null 2>&1

xdg-mime default tuxblox-player-dev.desktop x-scheme-handler/roblox
xdg-mime default tuxblox-player-dev.desktop x-scheme-handler/roblox-player
xdg-mime default tuxblox-studio-dev.desktop x-scheme-handler/roblox-studio
xdg-mime default tuxblox-studio-dev.desktop x-scheme-handler/roblox-studio-auth

echo "Registered this repo checkout (via launch.sh, prefix: $repoDir/build/runtime) as the"
echo "handler for roblox:, roblox-player:, roblox-studio:, and roblox-studio-auth: links."
echo "Run '$0 --uninstall' to remove."
