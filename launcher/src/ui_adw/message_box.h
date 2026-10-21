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

// How a message shown for another program ended. A button is named by where it sat, counting from the left, because the caller chose the set and so already knows what that position means.
struct MessageBoxOutcome {
    enum { Pressed, Dismissed, NoDisplay } how = NoDisplay;
    int button = -1;
};

// Shows a message another TuxBlox program asked for and says how it ended. The compatibility layer asks for these: a message from Roblox itself is drawn here because that side cannot link the interface stack, so it would otherwise look like a different program.
// buttons names one of the sets a Windows message box can offer, as its labels in order, separated by commas -- "OK", or "Cancel,OK", or "No,Yes". defaultButton is the one Enter presses, or -1 for the last.
// iconFd, when not -1, is an open file holding the icon for the window: width and height as 32-bit numbers, then rows of red, green, blue and alpha from the top down. It is the raising program's own icon, so the window is its window wherever the desktop shows one. It is read and closed here.
// canDismiss false leaves the box no way out but its buttons, which is what Windows does for a question that has no cancelling answer.
MessageBoxOutcome showRequestedMessageBox(const std::string &title, const std::string &message,
                                          const std::string &buttons, int defaultButton, bool canDismiss,
                                          int iconFd);

} // namespace tuxblox
