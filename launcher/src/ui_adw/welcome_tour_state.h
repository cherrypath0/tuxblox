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
#include <vector>

#include "settings.h"

namespace tuxblox {

// The tour's pages, in the order they are shown.
enum class TourPage { Welcome, Optional, Finish };

// Kept apart from the tour dialog so it can be tested without a display.
// An absent welcome_seen reads as false, so every install that predates the field sees the tour once.
bool shouldShowWelcome(const Settings& settings);

int tourPageCount();
// Clamped to the ends, so a carousel index can never name a page that is not there.
TourPage tourPageAt(int index);
int tourPageIndex(TourPage page);

// Skip goes to the last page rather than out, so the links are always reached.
int tourSkipTarget();

// One link on the last page.
struct TourLink {
    const char* pLabel;
    const char* pUrl;
};

// The same set, in the same order, as the About page's own links.
const std::vector<TourLink>& tourLinks();

} // namespace tuxblox
