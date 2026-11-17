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

class App;

// The first-run tour, in a fixed-size window of its own, shown before the launcher's.
// onDone runs once when that window goes: true for Get started, false for closing it without
// finishing. The caller decides what each means -- nothing is written or opened from here.
void presentWelcomeTour(App &app, GtkApplication *pGtkApp, std::function<void(bool finished)> onDone);

} // namespace tuxblox
