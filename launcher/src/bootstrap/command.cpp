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

#include "command.h"
#include "config.h"
#include "ui_adw.h"
#include "version.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace tuxblox {
namespace bootstrap {

std::optional<int> runCommand(int argc, char** argv) {
    if (argc < 2 || !isCommandFlag(argv[1])) return std::nullopt;

    const CliOptions options = parseArgs(argc, argv);
    switch (commandAction(options)) {
        case CommandAction::Version:
            printf("%s\n", kTuxBloxBuildId);
            return 0;
        case CommandAction::Usage:
            printf("%s", usageText());
            return 0;
        case CommandAction::Error:
            fprintf(stderr, "%s\n\n%s", options.error.c_str(), usageText());
            return 2;
        case CommandAction::Run:
            break;
    }

    // getenv returns char*; the config reader takes a const char* function so
    // a test can substitute its own environment.
    App app(options.mode, loadConfig([](const char* name) -> const char* {
        return getenv(name);
    }));

    // Started before any window exists: an update check is usually a single
    // network round trip ending in "already up to date", and opening a window
    // for that means a flash of one on every launch. It also means GTK is not
    // initialised at all in the common case.
    app.start();

    bool windowShown = false;
    while (!app.finished()) {
        if (app.needsWindow()) {
            windowShown = runAdwProgress(app);
            break;
        }
        // Nothing to draw, so this loop only has to notice the two flags above reasonably promptly.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // A window that never opened leaves the work running, so wait for it rather than abandoning a half-finished update.
    while (!windowShown && !app.finished()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    const Snapshot final = app.snapshot();
    // With no window there was nothing to show a failure on, so it goes to
    // the terminal instead -- the launcher captures this into the session log.
    if (!windowShown && final.phase == Phase::Error) {
        fprintf(stderr, "TuxBlox: %s\n", final.errorMessage.c_str());
    }
    // A cancel surfaces as the worker abandoning its download, which is not a failure to report.
    return final.phase == Phase::Error && !app.cancelRequested() ? 1 : 0;
}

} // namespace bootstrap
} // namespace tuxblox
