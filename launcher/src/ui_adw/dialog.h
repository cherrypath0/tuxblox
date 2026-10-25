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
#include <functional>
#include <string>
#include <vector>

// Third Parties
#include <adwaita.h>

namespace tuxblox {

// One button along the bottom of a dialog. The style class is libadwaita's own, so "suggested-action" or "destructive-action", or nullptr for an ordinary one. Pressing it closes the dialog and then runs onPress.
struct DialogButton {
    std::string label;
    const char *pStyleClass = nullptr;
    std::function<void()> onPress;
    bool isDefault = false;
};

// A dialog shaped like the installer's window: the TuxBlox logo beside a heading and a body, the buttons along the bottom right, and no header bar of its own. Enter presses the one marked isDefault, or the last one when none is. Present it with adw_dialog_present(), where a null parent gives it a window of its own.
// `pExtra` is appended under the body, for a dialog that has to show more than a sentence -- a list, say. Ownership passes to the dialog.
AdwDialog *buildDialog(const std::string &heading, const std::string &body, std::vector<DialogButton> buttons,
                        GtkWidget *pExtra = nullptr);

} // namespace tuxblox
