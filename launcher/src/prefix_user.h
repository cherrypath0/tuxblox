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

// The virtual drive's C: folder for an install.
std::string driveCDir(const std::string& installDir);

// This computer's account name, as Wine takes it: USER, else the password
// database. Not sanitised -- it is only ever compared against folder names
// that already exist.
std::string hostAccountName();

// The name of the account folder inside it, e.g. "cherry" in
// drive_c/users/cherry. The compatibility layer decides it; this reads back
// what it chose, so the two can never disagree. "user" when there is no drive
// yet.
//
// `preferred` settles it when the drive holds more than one account folder:
// the one matching it is the folder %USERPROFILE% names, so it is where Roblox
// will actually be. Falls back to "user" when none of them is the host's.
std::string prefixUserName(const std::string& driveCDir, const std::string& preferred);
std::string prefixUserName(const std::string& driveCDir);

// That folder's full path: where Roblox, its logs and its settings live.
std::string prefixUserDir(const std::string& installDir);

} // namespace tuxblox
