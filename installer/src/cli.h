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

#pragma once

#include "version.h"
#include <string>
#include "release_file.h"

namespace tuxblox {

struct CliOptions {
    // Remove the install instead of performing one. Passed by the launcher's
    // Settings tab.
    bool uninstall = false;
    // Report progress on the terminal instead of opening a window, so it works over SSH / with no display.
    bool headless = false;
    // Don't exec the launcher once the install succeeds. Independent of
    // --headless; both modes honor it.
    bool noLaunch = false;
    bool help = false;
    // Print the build version and exit. Matches `proton/main --version` and the
    // launcher's footer -- all three come from the same TUXBLOX_BUILD_VERSION.
    bool version = false;
    // Which channel's newest release to install. It defaults to stable rather than to anything
    // about this binary: one copy of the installer is served to every channel, so "the channel this
    // was built for" is only whichever channel happened to be building. The launcher passes
    // --channel on every handoff, so this default is reached by a bare installer somebody
    // downloaded, where stable is the right answer. "dev" is accepted on the command line and
    // stored as its new name, "experimental".
    std::string channel = "stable";
    // A specific release, for support: "3.6.0.1". Validated here rather than left to 404 later,
    // because the likely slip is a three-field version, which is a COMPONENT version now.
    std::string release;
    // A release file already on disk, which is how the launcher hands one over. It passes the
    // bytes it read rather than the release's number: two independent reads can straddle a pointer
    // move, and the record this installer writes last would then name something nobody decided.
    std::string releaseFile;
    // Where to install, for a manual install outside the home folder. Empty
    // means the default, which installDir() resolves. The launcher passes
    // this on a handoff so the installer cannot act on a different install
    // from the one that asked for it.
    std::string dir;
    // Installing as root leaves a folder the real user cannot write to, so
    // it has to be asked for rather than just happening.
    bool allowRoot = false;
    // Non-empty means the arguments were unusable: the caller should print
    // this and usageText() to stderr and exit non-zero. Every other field is
    // meaningless when this is set.
    std::string error;
};

// Parses the process argv. Flags may appear in any order; unknown flags and
// stray positional arguments are reported through CliOptions::error rather
// than ignored, so a typo can't silently change what the installer does.
CliOptions parseArgs(int argc, const char* const* argv);

// Where a run learns which release to install. The handoff file wins, then an explicitly named
// release, and otherwise the channel's newest -- which is what a bare installer someone downloaded
// does, because the version it was built for is its own component version and not a release at all.
struct ReleaseSource {
    enum class Kind { Latest, Named, File };
    Kind kind = Kind::Latest;
    std::string release;  // valid iff kind == Named
    std::string path;     // valid iff kind == File
};

ReleaseSource releaseSourceFor(const CliOptions& options);

// Human-readable usage block, printed for --help and for any usage error.
const char* usageText();

} // namespace tuxblox
