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

// Shows a blocking error and returns once it is dismissed, for processes that have no window of their own (the watcher, and errors before the launcher window exists). With no display it prints to stderr instead, which the session log records.
void showErrorMessageBox(const std::string &title, const std::string &message);

// The same popup with a second button beside OK, labelled actionLabel. True only if that button was pressed, so with no display it is always false.
bool showErrorMessageBoxWithAction(const std::string &title, const std::string &message, const std::string &actionLabel);

// How a message shown for another program ended.
enum class MessageBoxAnswer {
    Pressed,   // OK
    Dismissed, // Cancel, Escape, or the window closed
    NoDisplay  // nothing was drawn, so the caller still has to reach the person another way
};

// Shows a message another TuxBlox program asked for and says how it ended. The compatibility layer asks for these: a message from Roblox itself is drawn here because that side cannot link the interface stack, so it would otherwise look like a different program.
// iconFd, when not -1, is an open file holding the icon for the window: width and height as 32-bit numbers, then rows of red, green, blue and alpha from the top down. It is the raising program's own icon, so the window is its window wherever the desktop shows one. It is read and closed here.
MessageBoxAnswer showRequestedMessageBox(const std::string &title, const std::string &message, bool withCancel,
                                         bool cancelIsDefault, int iconFd);

} // namespace tuxblox
