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

#include "cli.h"
#include <string>

namespace tuxblox {

CliOptions parseArgs(int argc, const char* const* argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--uninstall") {
            options.uninstall = true;
        } else if (arg == "--headless") {
            options.headless = true;
        } else if (arg == "--nolaunch") {
            options.noLaunch = true;
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (arg == "--version") {
            options.version = true;
        } else if (arg == "--channel") {
            // Falling back to "stable" on a missing value would silently
            // install the wrong channel during a launcher upgrade handoff,
            // so a value-less --channel is a hard usage error.
            if (i + 1 >= argc) {
                options.error = "Missing value for --channel.";
                return options;
            }
            options.channel = argv[++i];
        } else {
            options.error = "Unrecognized argument: " + arg;
            return options;
        }
    }

    return options;
}

const char* usageText() {
    return "Usage: TuxBloxInstaller [options]\n"
           "\n"
           "Installs TuxBlox into ~/.tuxblox, upgrading an existing install in\n"
           "place if one is found, then starts the TuxBlox Launcher.\n"
           "\n"
           "Options:\n"
           "  --headless         Report progress on the terminal instead of\n"
           "                     opening a window. Needs no display.\n"
           "  --nolaunch         Don't start the launcher once the install\n"
           "                     finishes.\n"
           "  --uninstall        Remove TuxBlox from this system instead of\n"
           "                     installing it.\n"
           "  --channel <name>   Release channel to install from (default:\n"
           "                     stable).\n"
           "  --version          Show the build version and exit.\n"
           "  -h, --help         Show this help and exit.\n";
}

} // namespace tuxblox
