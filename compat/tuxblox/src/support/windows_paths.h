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

// Rewrites the system folders in a registry path value to the capitalisation
// Windows uses, so a program reading %LOCALAPPDATA% is told C:\Users and not
// C:\users. The value is taken and returned as the registry file spells it,
// quoted and with its backslashes doubled. Anything that is not a drive path
// comes back untouched, and the folders on the drive are never renamed.
std::string canonicalWindowsPath(const std::string& value);

// Corrects every path in a Wine .reg file the same way, in place. The file is
// left alone when there is nothing to correct, so a drive that is already right
// is not rewritten on every launch. Returns true when the file changed.
bool canonicaliseRegFile(const std::filesystem::path& file);

} // namespace tuxblox
