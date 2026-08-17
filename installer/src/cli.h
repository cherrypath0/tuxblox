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
#include <string>

namespace tuxblox {

struct CliOptions {
    // Remove the install instead of performing one. Passed by the launcher's
    // Settings tab.
    bool uninstall = false;
    // Report progress on the terminal instead of opening a window. SDL is
    // never initialized in this mode, so it works over SSH / with no display.
    bool headless = false;
    // Don't exec the launcher once the install succeeds. Independent of
    // --headless; both modes honor it.
    bool noLaunch = false;
    bool help = false;
    // Which /v1/<channel>/... release to install. "stable" unless the
    // launcher passed --channel during an upgrade handoff.
    std::string channel = "stable";
    // Non-empty means the arguments were unusable: the caller should print
    // this and usageText() to stderr and exit non-zero. Every other field is
    // meaningless when this is set.
    std::string error;
};

// Parses the process argv. Flags may appear in any order; unknown flags and
// stray positional arguments are reported through CliOptions::error rather
// than ignored, so a typo can't silently change what the installer does.
CliOptions parseArgs(int argc, const char* const* argv);

// Human-readable usage block, printed for --help and for any usage error.
const char* usageText();

} // namespace tuxblox
