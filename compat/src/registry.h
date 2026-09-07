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
//
// Portions derived from Proton's proton.py:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#pragma once
#include <filesystem>
#include <map>
#include <string>

namespace tuxblox {

// Reads a value out of a Wine .reg file. The key path is written the way the
// file spells it, with doubled backslashes. Returns an empty string when the
// key or value is not there.
std::string getRegValue(const std::filesystem::path& file, const std::string& key,
                        const std::string& name);

// Reads a value and replaces it in one pass. Returns the old value.
std::string replaceRegValue(const std::filesystem::path& file, const std::string& key,
                            const std::string& name, const std::string& newValue);

// Makes a key hold the given values, creating whatever is missing. Values map
// names to raw registry text, e.g. "\"255 255 255\"" or "dword:00000000".
// Existing values are replaced in place, missing ones appended, and the key
// itself added when the file does not have it. Returns true if the file
// was rewritten.
bool setRegKeyValues(const std::filesystem::path& file, const std::string& key,
                     const std::map<std::string, std::string>& values);

} // namespace tuxblox
