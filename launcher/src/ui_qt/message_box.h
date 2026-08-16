// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <string>

namespace tuxblox {

// Shows a native-ish blocking QMessageBox and returns once dismissed.
// Callable from a process that never constructs a QApplication of its own
// (e.g. the --watch-launch helper) -- if none exists yet on this thread, a
// minimal local one is constructed first. Replaces every
// SDL_Init(SDL_INIT_VIDEO) + SDL_ShowSimpleMessageBox + SDL_Quit call site
// that existed under launcher/src/ before this migration (main.cpp's
// single-instance error, ui.cpp's Distrobox container warning,
// watch_launch.cpp's crash/launch-failure popups).
void showErrorMessageBox(const std::string& title, const std::string& message);
void showWarningMessageBox(const std::string& title, const std::string& message);

} // namespace tuxblox
