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
#include <cstddef>
#include <functional>
#include <string>

// Third Parties
#include <gtk/gtk.h>

namespace tuxblox {

// One of the launcher's white PNG icons, drawn in the text colour of wherever it is shown so it stays visible in light and dark. Empty, not null, if the bytes are not a PNG. The caller owns the reference.
GdkPaintable *symbolicIcon(const unsigned char *pPng, size_t length);

GtkWidget *iconImage(const unsigned char *pPng, size_t length, int pixelSize);

// libadwaita reads row titles as markup, so an ampersand in a channel name or a message would otherwise blank the row
GtkWidget *plainActionRow(const std::string &title, const std::string &subtitle);
GtkWidget *plainSwitchRow(const std::string &title, const std::string &subtitle);

// Wrapped and selectable, so a failure can be copied into a bug report; hidden until showError() gives it something to say
GtkWidget *errorLabel();
void showError(GtkWidget *pLabel, const std::string &message);

// Setters that do nothing when the text is unchanged, since pages call them on every poll
void setLabelText(GtkLabel *pLabel, const std::string &text);
void setButtonLabel(GtkButton *pButton, const std::string &text);

// Styles the launcher adds on top of libadwaita's: the green Launch button and the yellow-orange Update button. Call once the display is up.
void installLauncherStyles();

void showNotice(GtkWidget *pParent, const std::string &heading, const std::string &body);

// onConfirm runs only if the destructive button is pressed; Escape and closing both count as cancel
void confirmDestructive(GtkWidget *pParent, const std::string &heading, const std::string &body,
                        const std::string &confirmLabel, std::function<void()> onConfirm);

} // namespace tuxblox
