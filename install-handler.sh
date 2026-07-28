#!/bin/bash
# Registers roblox-player:, roblox-studio:, and roblox-studio-auth: as Linux
# URI-scheme handlers pointing at this checkout's launch.sh, so "Play"/"Edit"
# on roblox.com (and Studio's browser-based login flow, which redirects back
# via roblox-studio-auth:) launches TuxBlox instead of doing nothing. Run
# once, manually. Not called from build.sh/launch.sh since it's optional
# desktop integration, not a build dependency.
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
Name=TuxBlox Player Handler
Exec=$repoDir/launch.sh player %u
NoDisplay=true
Terminal=false
MimeType=x-scheme-handler/roblox-player;
EOF

cat > "$studioDesktop" <<EOF
[Desktop Entry]
Type=Application
Name=TuxBlox Studio Handler
Exec=$repoDir/launch.sh studio %u
NoDisplay=true
Terminal=false
MimeType=x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;
EOF

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$appsDir" >/dev/null 2>&1

xdg-mime default tuxblox-player.desktop x-scheme-handler/roblox-player
xdg-mime default tuxblox-studio.desktop x-scheme-handler/roblox-studio
xdg-mime default tuxblox-studio.desktop x-scheme-handler/roblox-studio-auth

echo "Registered TuxBlox as the handler for roblox-player:, roblox-studio:, and roblox-studio-auth: links."
echo "Run '$0 --uninstall' to remove."
