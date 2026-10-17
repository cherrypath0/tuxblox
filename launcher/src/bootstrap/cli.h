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
namespace bootstrap {

// What this run was asked to do. None means the arguments named no mode.
enum class Mode { None, Preview, Install, Update };

struct CliOptions {
    Mode mode = Mode::None;
    bool help = false;
    bool version = false;
    // Non-empty means the arguments were unusable: print this and
    // usageText() to stderr and exit non-zero. Every other field is
    // meaningless when this is set.
    std::string error;
};

// Parses the process argv. Unknown flags, stray positional arguments and two
// modes at once are all reported through CliOptions::error rather than
// ignored, so a typo cannot quietly change what the bootstrapper does.
CliOptions parseArgs(int argc, const char* const* argv);

// What a parsed command line asks for. Answering help or the version is not running
// anything, and must not be mistaken for it: --help beside a real mode would otherwise
// start a multi-gigabyte download.
enum class CommandAction { Run, Usage, Version, Error };

CommandAction commandAction(const CliOptions& options);

// True for the three flags that belong to this half of the launcher, so main() can
// hand them over before it does anything else.
bool isCommandFlag(const std::string& arg);

// Human-readable usage block, printed for --help and for any usage error.
const char* usageText();

} // namespace bootstrap
} // namespace tuxblox
