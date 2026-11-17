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

#include "version.h"
#include <vector>

namespace tuxblox {

namespace {

// The runtime page is first and exists only in a Flatpak, which is the only build that fetches its
// own compatibility layer: everywhere else the installer has already put one there.
const std::vector<TourPage>& pages() {
    static const std::vector<TourPage> list = [] {
        std::vector<TourPage> out;
        if (kFlatpakBuild) out.push_back(TourPage::Runtime);
        out.push_back(TourPage::Welcome);
        out.push_back(TourPage::Optional);
        out.push_back(TourPage::Finish);
        return out;
    }();
    return list;
}

} // namespace

bool shouldShowWelcome(const Settings& settings) {
    return !settings.welcomeSeen;
}

bool tourMayAdvance(TourPage page, bool runtimeMissing) {
    return page != TourPage::Runtime || !runtimeMissing;
}

int tourPageCount() {
    return static_cast<int>(pages().size());
}

TourPage tourPageAt(int index) {
    if (index <= 0) return pages().front();
    if (index >= tourPageCount()) return pages().back();
    return pages()[static_cast<size_t>(index)];
}

int tourPageIndex(TourPage page) {
    for (int at = 0; at < tourPageCount(); ++at) {
        if (pages()[static_cast<size_t>(at)] == page) return at;
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
