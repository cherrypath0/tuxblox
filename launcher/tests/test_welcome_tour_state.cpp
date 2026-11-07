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

#include "ui_adw/welcome_tour_state.h"
#include "settings.h"

#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    using namespace tuxblox;

    // A fresh install, and every install that predates the field, sees the tour.
    {
        Settings s;
        assert(shouldShowWelcome(s));
    }
    {
        Settings s;
        s.welcomeSeen = true;
        assert(!shouldShowWelcome(s));
    }

    assert(tourPageCount() == 3);
    assert(tourPageAt(0) == TourPage::Welcome);
    assert(tourPageAt(2) == TourPage::Finish);
    assert(tourPageIndex(TourPage::Optional) == 1);

    // Skip lands on the last page rather than closing, so the links are always reached.
    assert(tourSkipTarget() == tourPageCount() - 1);

    // The six links the About page carries, in its order.
    {
        const std::vector<TourLink> &links = tourLinks();
        assert(links.size() == 6);
        assert(std::strcmp(links[0].pLabel, "Website") == 0);
        assert(std::strcmp(links[0].pUrl, "https://tuxblox.net") == 0);
        assert(std::strcmp(links[5].pLabel, "Terms of Use") == 0);
        assert(std::strcmp(links[5].pUrl, "https://tuxblox.net/terms") == 0);
    }

    std::printf("welcome_tour_state: all tests passed\n");
    return 0;
}
