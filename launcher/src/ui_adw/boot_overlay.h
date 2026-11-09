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

// Third Parties
#include <gtk/gtk.h>

namespace tuxblox {

struct BootOverlay;

// The opening animation, drawn as an overlay child of the main window so it
// covers the launcher's normal content in the same window. Returns null when
// the logo cannot be drawn or the desktop asks for reduced motion; the caller
// should then show the content immediately.
//
// Add bootOverlayWidget() to the window's GtkOverlay, present the window, and
// let onFinished run when the entrance has played. onFinished may destroy the
// overlay; it is never run after bootOverlayDestroy().
BootOverlay *bootOverlayCreate(std::function<void()> onFinished);

GtkWidget *bootOverlayWidget(BootOverlay *pOverlay);

// Stops the animation and destroys the drawing area. Safe to call from
// onFinished and safe when the window was already destroyed.
void bootOverlayDestroy(BootOverlay *pOverlay);

} // namespace tuxblox
