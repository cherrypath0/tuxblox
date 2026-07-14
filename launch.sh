#!/bin/bash
cd "$(dirname "$0")" || exit 1
mkdir -p ~/.local/share/tuxblox/steamapps
mkdir -p prefix
export STEAM_COMPAT_DATA_PATH="$(pwd)/prefix"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/tuxblox"

choice="$1"

if [ -z "$choice" ]; then
    echo "Which do you want to launch? [0/p/player = Player (default), 1/s/studio = Studio]"
    read -r choice
fi

case "$choice" in
    ""|0|p|P|player|Player|PLAYER)
        exePath="RobloxPlayer/RobloxPlayerBeta.exe"
        label="Roblox Client"
        ;;
    1|s|S|studio|Studio|STUDIO)
        exePath="RobloxStudio/RobloxStudioInstaller.exe"
        label="Roblox Studio"
        ;;
    *)
        exePath="RobloxPlayer/RobloxPlayerBeta.exe"
        label="Roblox Client"
        ;;
esac

echo "Launching $label"
PROTON_LOG=1 "$(pwd)/ProtonBuild/dist/proton" run "$exePath"
echo "Exit code: $?"