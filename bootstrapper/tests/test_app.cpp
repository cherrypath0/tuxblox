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

#include "app.h"
#include "cli.h"
#include "config.h"

#include <cassert>
#include <cstdio>

int main() {
    using namespace tuxblox;

    Config config;
    config.installDir = "/nonexistent/tuxblox-test";
    config.channel = "live";

    // An update check runs before every launch and usually finds nothing to
    // do, so it starts headless. A window would otherwise flash on screen on
    // each launch. The thread is never started here -- this is the state the
    // window loop in main() sees before any work happens.
    {
        App app(Mode::Update, config);
        assert(!app.needsWindow());
        assert(!app.holdsOpenWhenDone());
    }

    // Every other mode was asked for directly, so its window belongs on
    // screen from the start rather than after a network round trip.
    {
        App app(Mode::Install, config);
        assert(app.needsWindow());
        assert(!app.holdsOpenWhenDone());
    }
    {
        App app(Mode::Preview, config);
        assert(app.needsWindow());
        // Preview is there to be read, so it stays up once it is finished.
        assert(app.holdsOpenWhenDone());
    }
    {
        App app(Mode::None, config);
        assert(app.needsWindow());
    }

    printf("app: all tests passed\n");
    return 0;
}
