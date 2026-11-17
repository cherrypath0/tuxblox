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

// That folder's full path inside the drive.
std::string prefixUserDir(const std::string& installDir);

// Where Roblox's own data is: its installed builds, its settings and its logs.
//
// Inside the drive Roblox always finds it at <account folder>/AppData/Local/Roblox, and
// TuxBlox points that at wherever the data is really being kept -- a folder in the
// install folder normally, or the one the desktop names for application data when the
// experimental Linux integration is turned on. So this follows it: what the
// compatibility layer chose is read back rather than worked out a second time, which is
// what stops two programs disagreeing about where Roblox's data is.
//
// Before the drive has been built nothing points anywhere yet, and then the folder
// inside the drive is the answer -- which is where a first install puts things, and what
// the layer moves out to wherever it belongs the next time it sets the drive up.
std::string robloxDataDir(const std::string& installDir);

// The same, for a caller that already has the account folder inside the drive.
std::string robloxDataDirIn(const std::string& accountDir);

} // namespace tuxblox
