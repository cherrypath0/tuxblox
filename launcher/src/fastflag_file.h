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
#include "lnk_resolver.h"
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

// The flags the Graphics setting forces for `graphics`: the chosen renderer's flag True and the other two False, so a renderer is asked for rather than merely not ruled out.
// Empty for "automatic" and for anything unrecognised, which writes nothing and leaves the choice to Roblox and to the FastFlags page.
std::vector<FastFlag> graphicsFlags(const std::string& graphics);

// `flags` with graphicsFlags() written over it. The setting wins wherever the two name the same flag, including over a row the user typed, since a renderer chosen in Settings is meant to be the one that applies.
std::vector<FastFlag> withGraphicsChoice(const std::vector<FastFlag>& flags, const std::string& graphics);

// writeClientAppSettings() for every installed version of `target`, not only the one about to start: a deleted flag is only really deleted once no version directory still carries it, and the launcher can start a version other than the pinned one. Call it on every path that launches Roblox.
// A version directory that cannot be written is skipped, since launching without the overrides beats not launching at all.
void applyFastFlags(const std::string& installDir, LaunchTarget target, const Settings& settings);

} // namespace tuxblox
