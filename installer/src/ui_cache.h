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

// The folder TuxBlox owns under the user's cache: $XDG_CACHE_HOME when it names an absolute path, else $HOME/.cache, then "tuxblox". Throws std::runtime_error when neither is usable.
std::string uiCacheRoot();

// Where an extracted interface stack for `version` lives. The payload digest is part of the name, so a different payload can never land on another build's folder.
std::string uiCacheDir(const std::string& version, const std::string& sha256);

// True when `dir` holds a finished extraction of the payload whose digest is `sha256`.
bool uiCacheIsComplete(const std::string& dir, const std::string& sha256);

// Records `sha256` so a later run can tell this extraction is finished and current.
void uiCacheMarkComplete(const std::string& dir, const std::string& sha256);

// Removes every ui-* folder beside `keepDir`, and nothing else. Best effort: a folder that cannot be removed is left alone.
void uiCachePruneOthers(const std::string& keepDir);

} // namespace tuxblox
