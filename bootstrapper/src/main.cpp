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

#include <cstdio>
#include <cstdlib>

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

    Ui ui;
    if (!ui.init()) {
        fprintf(stderr, "Could not open a window.\n");
        return 1;
    }

    app.start();
    while (ui.renderFrame(app)) {
    }
    ui.shutdown();

    const Snapshot final = app.snapshot();
    return final.phase == Phase::Error ? 1 : 0;
}
