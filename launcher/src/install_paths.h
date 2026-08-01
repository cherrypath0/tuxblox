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

// Returns the fixed TuxBlox install directory: $HOME/.local/share/tuxblox
// Throws std::runtime_error if HOME is not set.
std::string installDir();

// True if the filesystem containing `path` has at least `minBytes` free.
bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes);

// installDir + "/ProtonBuild"
std::string protonBuildDirUnder(const std::string& installDir);

// protonBuildDirUnder(installDir) + "/dist/version"
std::string protonVersionFilePathUnder(const std::string& installDir);

// Reads protonVersionFilePathUnder(installDir), a "<epoch> <version>" text
// file written by the root build.sh. Returns just the version token, or
// std::nullopt if the file is missing or doesn't contain two
// whitespace-separated tokens.
std::optional<std::string> readInstalledProtonVersion(const std::string& installDir);

} // namespace tuxblox
