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

// An icon turned into dots at the size it will actually be drawn at, which is what keeps it from looking soft. A drawing is rendered straight at that size; a picture is resized properly here rather than left to the renderer to shrink in one step.
// A drawing has no size of its own, so a `pixelSize` of 0 gets nothing back from one, where a picture is handed over as it is. The caller owns the returned reference.
GdkTexture *iconTexture(const unsigned char *pPng, size_t length, int pixelSize);

// The same, as a paintable, which also keeps the window free of any icon theme. The caller owns the returned reference.
GdkPaintable *logoPaintable(const unsigned char *pPng, size_t length, int pixelSize);

} // namespace tuxblox
