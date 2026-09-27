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
#include <filesystem>
#include <string>

namespace tuxblox {

// The account name the virtual drive's user folder is named after, taken from
// the host account. Falls back to "user" for a name Windows could not have.
std::string accountName();

// The rule on its own. Wine's own copy of it decides the folder name, and this
// has to answer identically -- the shared table in the tests is what says so.
std::string sanitizedAccountName(const std::string& raw);

// The account folder that is actually in the drive, which is whatever Wine
// created. Returns "user" when the drive has none, and an empty string when it
// somehow has two, which the caller reports rather than guessing between.
std::string prefixAccountName(const std::filesystem::path& prefixDir);

} // namespace tuxblox
