#!/bin/sh
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

# /usr/bin/tuxblox -- run the per-user TuxBlox install if one exists,
# otherwise hand off to the system-packaged bootstrap installer. The
# installer performs the same per-user ~/.tuxblox install the official
# website download does, then execs the launcher itself; from that point
# on the per-user install manages its own updates.
if [ -x "${HOME}/.tuxblox/TuxBloxLauncher" ]; then
    exec "${HOME}/.tuxblox/TuxBloxLauncher" "$@"
fi
exec /usr/bin/tuxblox-installer "$@"
