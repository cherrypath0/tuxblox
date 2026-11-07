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

#include "welcome_tour_state.h"

namespace tuxblox {

namespace {

const TourPage Pages[] = {TourPage::Welcome, TourPage::Optional, TourPage::Finish};

} // namespace

bool shouldShowWelcome(const Settings& settings) {
    return !settings.welcomeSeen;
}

int tourPageCount() {
    return static_cast<int>(sizeof(Pages) / sizeof(Pages[0]));
}

TourPage tourPageAt(int index) {
    if (index <= 0) return Pages[0];
    if (index >= tourPageCount()) return Pages[tourPageCount() - 1];
    return Pages[index];
}

int tourPageIndex(TourPage page) {
    for (int at = 0; at < tourPageCount(); ++at) {
        if (Pages[at] == page) return at;
    }
    return 0;
}

int tourSkipTarget() {
    return tourPageCount() - 1;
}

const std::vector<TourLink>& tourLinks() {
    // Copied from page_about.cpp's Links[] so the two cannot drift apart.
    static const std::vector<TourLink> links = {
        {"Website", "https://tuxblox.net"},
        {"Documentation", "https://tuxblox.net/docs"},
        {"GitHub", "https://tuxblox.net/github"},
        {"Discord", "https://tuxblox.net/discord"},
        {"Privacy Policy", "https://tuxblox.net/privacy"},
        {"Terms of Use", "https://tuxblox.net/terms"},
    };
    return links;
}

} // namespace tuxblox
