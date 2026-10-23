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
#include "install_paths.h"
#include <string>

namespace tuxblox {

namespace {

// Four dot-separated numbers. A component version is three, and mistaking one for the other is the
// slip this exists to catch.
bool isReleaseVersion(const std::string& value) {
    int fields = 1;
    size_t digits = 0;
    for (char c : value) {
        if (c == '.') {
            if (digits == 0) return false;
            digits = 0;
            fields++;
        } else if (c >= '0' && c <= '9') {
            digits++;
        } else {
            return false;
        }
    }
    return digits > 0 && fields == 4;
}

} // namespace

ReleaseSource releaseSourceFor(const CliOptions& options) {
    if (!options.releaseFile.empty()) return {ReleaseSource::Kind::File, "", options.releaseFile};
    if (!options.release.empty()) return {ReleaseSource::Kind::Named, options.release, ""};
    return {};
}

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
        } else if (arg == "--latest") {
            // What this now names is the default. Still accepted so a handoff from a launcher that
            // predates this change keeps working rather than failing on an unknown flag.
        } else if (arg == "--release") {
            if (i + 1 >= argc) {
                options.error = "Missing value for --release.";
                return options;
            }
            options.release = argv[++i];
            if (!isReleaseVersion(options.release)) {
                options.error = "--release needs a release version like 3.6.0.1, not \"" +
                                 options.release + "\".";
                return options;
            }
        } else if (arg == "--release-file") {
            if (i + 1 >= argc) {
                options.error = "Missing value for --release-file.";
                return options;
            }
            options.releaseFile = argv[++i];
        } else if (arg == "--channel") {
            // Falling back to "stable" on a missing value would silently
            // install the wrong channel during a launcher upgrade handoff,
            // so a value-less --channel is a hard usage error.
            if (i + 1 >= argc) {
                options.error = "Missing value for --channel.";
                return options;
            }
            // "dev" was renamed to "experimental". Accepted as an alias so an
            // old script, or an older launcher handing off during an upgrade,
            // does not end up asking the server for a channel that no longer
            // exists and reading back "no releases yet".
            const std::string value = argv[++i];
            options.channel = value == "dev" ? "experimental" : value;
        } else if (arg == "--allow-root") {
            options.allowRoot = true;
        } else if (arg == "--dir") {
            if (i + 1 >= argc) {
                options.error = "Missing value for --dir.";
                return options;
            }
            // A relative path would resolve against the current directory, which a double-clicked installer does not meaningfully have.
            const std::string value = argv[++i];
            if (value.empty() || value[0] != '/') {
                options.error = "--dir needs an absolute path.";
                return options;
            }
            options.dir = normalizedDir(value);
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
           "Installs TuxBlox, upgrading an existing install in place if one is\n"
           "found, then starts the TuxBlox Launcher. Installs into ~/.tuxblox\n"
           "unless --dir says otherwise. Installs the version this installer was\n"
           "newest release on its channel.\n"
           "\n"
           "Options:\n"
           "  --dir <path>       Absolute path to install into, instead of\n"
           "                     ~/.tuxblox.\n"
           "  --allow-root       Allow installing as root, which is not\n"
           "                     recommended.\n"
           "  --headless         Report progress on the terminal instead of\n"
           "                     opening a window. Needs no display.\n"
           "  --nolaunch         Don't start the launcher once the install\n"
           "                     finishes.\n"
           "  --uninstall        Remove TuxBlox from this system instead of\n"
           "                     installing it.\n"
           "  --channel <name>   Release channel to install from (default: the\n"
           "                     channel this installer was built for).\n"
           "  --release <ver>    Install a specific release, e.g. 3.6.0.1, instead\n"
           "                     of the newest one.\n"
           "  --release-file <p> Install the release described by a file already on\n"
           "                     disk. Used by the launcher when it hands over an\n"
           "                     update it has already resolved.\n"
           "  --latest           Accepted and ignored: installing the newest release\n"
           "                     instead of this installer's own version.\n"
           "  --version          Show the build version and exit.\n"
           "  -h, --help         Show this help and exit.\n";
}

} // namespace tuxblox
