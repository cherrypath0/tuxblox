#!/bin/bash
set -e

cd "$(dirname "$0")"

robloxPlayerUrl="https://rdd.tuxblox.net/download?noconsole=true&binary=WindowsPlayer&channel=production&version="
robloxStudioUrl="https://setup.rbxcdn.com/RobloxStudioInstaller.exe"

playerDir="RobloxPlayer"
studioDir="RobloxStudio"

playerZip="robloxPlayer.zip"

userAgent="TuxBlox-Client/1.0"

if [ -d "$playerDir" ] || [ -d "$studioDir" ]; then
    echo "WARNING: You already have RobloxPlayer and/or RobloxStudio directories installed!"
    echo "This script will completely overwrite those directories."
    read -p "Are you sure you want to execute this script? [Y/n]: " confirmOverwrite
    confirmOverwrite=${confirmOverwrite:-Y}
    if [[ ! "$confirmOverwrite" =~ ^[Yy]$ ]]; then
        echo ":: Aborting install."
        exit 0
    fi
fi

echo ":: Removing existing directories (if present)"
rm -rf "$playerDir" "$studioDir"

echo ":: Creating fresh directories"
mkdir -p "$playerDir" "$studioDir"

echo ":: Downloading RobloxPlayer"
curl -A "$userAgent" -L -o "$playerZip" "$robloxPlayerUrl"

echo ":: Downloading RobloxStudio"
curl -A "$userAgent" -L -o "RobloxStudioInstaller.exe" "$robloxStudioUrl"

echo ":: Extracting RobloxPlayer"
unzip -oq "$playerZip" -d "$playerDir"

echo ":: Moving RobloxStudioInstaller"
mv RobloxStudioInstaller.exe "$studioDir"

echo ":: Cleaning up downloaded archives"
rm -f "$playerZip"

echo ":: Install complete."