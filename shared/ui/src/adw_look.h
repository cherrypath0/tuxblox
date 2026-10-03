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
#include <string>

// Third Parties
#include <gdk/gdk.h>

namespace tuxblox {

// Dark, and the bundled font, so the window looks the same on every desktop instead of inheriting the host's. Call once GTK is up.
void applyLook();

// The colours every TuxBlox window wears: "system" follows the desktop, "dark" and "light" pin it to one of them, and "grey" leaves libadwaita's own palette alone. Call it again whenever the choice changes; open windows redraw themselves.
void applyTheme(const std::string &theme);

// The theme chosen in the launcher's settings, for the programs that have no settings of their own to hand. "system" whenever there is nothing readable to go on, which is what a fresh install looks like.
std::string settingsTheme();

// The PNG resized to the size it will actually be drawn at. A renderer shrinking a 440-pixel image into a 72-pixel box in one step is what makes an icon look soft, so the resizing is done properly here instead. Pass 0 to keep the image as it is; the caller owns the returned reference.
GdkTexture *iconTexture(const unsigned char *pPng, size_t length, int pixelSize);

// The PNG as a paintable, which also keeps the window free of any icon theme. The caller owns the returned reference.
GdkPaintable *logoPaintable(const unsigned char *pPng, size_t length, int pixelSize);

} // namespace tuxblox
