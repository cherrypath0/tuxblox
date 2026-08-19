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
#include <cstdint>
#include <optional>
#include <string>

namespace tuxblox {

// Returns the fixed TuxBlox install directory: $HOME/.tuxblox
// Throws std::runtime_error if HOME is not set.
std::string installDir();

// True if the filesystem containing `path` has at least `minBytes` free.
bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes);

// installDir + "/proton"
std::string protonDirUnder(const std::string& installDir);

// Runs `<installDir>/proton/main --version` (the build version is baked
// into the binary by the root build.sh) and returns its first output line,
// or std::nullopt if the binary is missing, exits nonzero, or prints
// nothing.
std::optional<std::string> readInstalledProtonVersion(const std::string& installDir);

// The currently-running binary's real on-disk path, via /proc/self/exe -- not
// argv[0], which can be relative, a bare basename, or missing entirely
// depending on how a .desktop Exec= line or a shell invoked it. This is what a
// self-update rename()s over, what execv() re-launches, and what the exported
// .desktop entries name in their Exec= lines. Returns "" only if /proc is
// unavailable, which cannot happen for the calling process on Linux.
std::string selfExePath();

} // namespace tuxblox
