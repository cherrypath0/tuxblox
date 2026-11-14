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

# Installs what a build needs on the host. make itself has to be there already to run this. Everything else is built inside the container, so this
# is podman and the handful of tools used to fill it and to pack what comes out.
#
# Each component's build.sh keeps its own copy of this for the case where it is run on its own.

set -eo pipefail

detectPkgManager() {
    if command -v apt-get >/dev/null 2>&1; then echo apt
    elif command -v dnf >/dev/null 2>&1; then echo dnf
    elif command -v pacman >/dev/null 2>&1; then echo pacman
    elif command -v brew >/dev/null 2>&1; then echo brew
    elif command -v apk >/dev/null 2>&1; then echo apk
    else echo unknown
    fi
}

echo ":: Installing host build dependencies"
case "$(detectPkgManager)" in
    apt)
        sudo apt-get update
        sudo apt-get install -y podman curl git uidmap python3 zstd gcc
        ;;
    dnf)
        sudo dnf install -y podman curl git python3 zstd gcc
        ;;
    pacman)
        sudo pacman -S --needed --noconfirm podman curl git python zstd gcc
        ;;
    brew)
        brew install podman curl git python3 zstd gcc
        ;;
    apk)
        sudo apk add podman curl git uidmap python3 zstd gcc
        ;;
    *)
        echo "!! Unknown package manager. Install manually: podman, curl, git, python3, zstd, gcc" >&2
        exit 1
        ;;
esac
