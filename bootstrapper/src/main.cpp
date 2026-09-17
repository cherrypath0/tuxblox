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
#include "ui.h"
#include "version.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main(int argc, char** argv) {
    using namespace tuxblox;

    const CliOptions options = parseArgs(argc, argv);
    if (options.version) {
        printf("%s\n", kTuxBloxBuildId);
        return 0;
    }
    if (options.help) {
        printf("%s", usageText());
        return 0;
    }
    if (!options.error.empty()) {
        fprintf(stderr, "%s\n\n%s", options.error.c_str(), usageText());
        return 2;
    }

    // getenv returns char*; the config reader takes a const char* function so
    // a test can substitute its own environment.
    App app(options.mode, loadConfig([](const char* name) -> const char* {
        return getenv(name);
    }));

    // Started before any window exists: an update check is usually a single
    // network round trip ending in "already up to date", and opening a window
    // for that means a flash of one on every launch. It also skips creating an
    // SDL window, a GL context and an ImGui context in the common case.
    app.start();

    Ui ui;
    bool windowOpen = false;
    while (true) {
        if (!windowOpen && app.needsWindow()) {
            if (!ui.init()) {
                fprintf(stderr, "Could not open a window.\n");
                app.cancel();
                return 1;
            }
            windowOpen = true;
        }

        if (windowOpen) {
            if (!ui.renderFrame(app)) break;
        } else {
            if (app.finished()) break;
            // Nothing to draw, so this loop only has to notice the two flags
            // above reasonably promptly.
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    if (windowOpen) ui.shutdown();

    const Snapshot final = app.snapshot();
    // With no window there was nothing to show a failure on, so it goes to
    // the terminal instead -- the launcher captures this into the session log.
    if (!windowOpen && final.phase == Phase::Error) {
        fprintf(stderr, "TuxBlox: %s\n", final.errorMessage.c_str());
    }
    return final.phase == Phase::Error ? 1 : 0;
}
