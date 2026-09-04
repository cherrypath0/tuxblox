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
#include "settings.h"
#include <string>
#include <vector>

namespace tuxblox {

// Roblox reads FastFlag overrides from a ClientSettings folder inside the
// version's own directory, so the file has to be rewritten whenever the
// active version changes -- which is why it is written at launch rather than
// when the user edits a flag.
inline constexpr const char* kClientSettingsDirName = "ClientSettings";
inline constexpr const char* kClientAppSettingsFileName = "ClientAppSettings.json";

// The JSON object Roblox reads, built from `flags`. Every value is written
// as a string, whatever it looks like: Roblox accepts that for all flag
// types, so the editor never has to guess whether "60" means the number or
// the text. Names are written in the order given; a repeated name collapses
// to a single key holding the last value, since a JSON object cannot hold
// both.
std::string renderClientAppSettings(const std::vector<FastFlag>& flags);

// Writes renderClientAppSettings() to
// <versionDir>/ClientSettings/ClientAppSettings.json, creating the folder if
// it isn't there. Deletes the file instead when `flags` is empty, so clearing
// every flag restores stock behaviour rather than leaving an empty override
// behind. Returns false if the folder or file could not be written.
bool writeClientAppSettings(const std::string& versionDir, const std::vector<FastFlag>& flags);

} // namespace tuxblox
