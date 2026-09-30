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

// Third Parties
#include <gdk/gdk.h>

namespace tuxblox {

// Dark, and the bundled font, so the window looks the same on every desktop instead of inheriting the host's. Call once GTK is up.
void applyLook();

// The PNG as a paintable, which also keeps the window free of any icon theme. The caller owns the returned reference.
GdkPaintable *logoPaintable(const unsigned char *pPng, size_t length);

} // namespace tuxblox
