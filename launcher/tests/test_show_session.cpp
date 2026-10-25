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

// Open on a Home row asks for one session by name. The Sessions page takes that request on its next
// pass, and taking it is what marks it answered -- a request left behind would re-select the same
// session on every pass afterwards, and clicking any other row in the list would never stick.

#include "app.h"

#include <cassert>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    const fs::path work = fs::temp_directory_path() / "tuxblox_test_show_session";
    fs::remove_all(work);
    fs::create_directories(work / "install");

    tuxblox::App app((work / "install").string(), (work / "launcher").string());

    // Nothing has been asked for.
    assert(app.takeSessionToShow() == 0);
    printf("  idle: nothing asked for\n");

    app.showSession(4242);
    assert(app.snapshot().activeTab == tuxblox::Tab::Sessions);
    printf("  asking for a session opens the Sessions tab\n");

    assert(app.takeSessionToShow() == 4242);
    assert(app.takeSessionToShow() == 0);
    printf("  the request is answered once, then gone\n");

    // A second ask replaces the first rather than queuing behind it: only the latest click matters.
    app.showSession(11);
    app.showSession(22);
    assert(app.takeSessionToShow() == 22);
    assert(app.takeSessionToShow() == 0);
    printf("  the latest ask wins\n");

    fs::remove_all(work);
    printf("show_session: all tests passed\n");
    return 0;
}
