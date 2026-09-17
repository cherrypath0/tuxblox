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

namespace tuxblox {

namespace {

bool setMode(CliOptions& options, Mode mode) {
    if (options.mode != Mode::None) {
        options.error = "Choose one of --preview, --install or --update.";
        return false;
    }
    options.mode = mode;
    return true;
}

} // namespace

CliOptions parseArgs(int argc, const char* const* argv) {
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--preview") {
            if (!setMode(options, Mode::Preview)) return options;
        } else if (arg == "--install") {
            if (!setMode(options, Mode::Install)) return options;
        } else if (arg == "--update") {
            if (!setMode(options, Mode::Update)) return options;
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (arg == "--version") {
            options.version = true;
        } else {
            options.error = "Unrecognised argument: " + arg;
            return options;
        }
    }

    if (options.help || options.version) return options;
    if (options.mode == Mode::None) {
        options.error = "Nothing to do: pass --preview, --install or --update.";
    }
    return options;
}

const char* usageText() {
    return "Usage: TuxBloxBootstrapper [--preview | --install | --update]\n"
           "\n"
           "  --preview   Show the window without downloading or writing anything.\n"
           "  --install   Install the Roblox version the environment describes.\n"
           "  --update    Install the channel's newest version if it is missing.\n"
           "              Shows no window unless there is something to download.\n"
           "  --help      Show this message.\n"
           "  --version   Print the TuxBlox version and exit.\n"
           "\n"
           "Environment:\n"
           "  TUXBLOX_BOOTSTRAPPER_DOWNLOAD_SERVER      Mirror to download from.\n"
           "  TUXBLOX_BOOTSTRAPPER_DOWNLOAD_RBXHASH     Exact version, or blank for newest.\n"
           "  TUXBLOX_BOOTSTRAPPER_DOWNLOAD_RBXCHANNEL  Roblox release channel.\n"
           "  TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP         player or studio.\n"
           "  TUXBLOX_BOOTSTRAPPER_INSTALL_DIR          Where versions are installed.\n";
}

} // namespace tuxblox
