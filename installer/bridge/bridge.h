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
#include <vector>

namespace tuxblox {
namespace bridge {

// The release this bridge fetches its installer from. Frozen on purpose: a client old enough to
// need the bridge cannot read the current layout, and the installer in this release can, so it is
// the one that carries them across. Anything after 3.9.11.0 would do; this is the newest verified.
const char* pinnedRelease();

// The address that release publishes its installer at, newest CDN first.
std::vector<std::string> installerUrls();

// What the fetched installer must hash to. Checked before it is ever run.
const char* installerSha256();

// Whether `argv` asks the bridge itself a question rather than asking for an install.
bool answersItself(const std::vector<std::string>& argv);

// --version is read back by the launcher, which takes the first line as the version of whatever it
// ran, so it must answer a bare version and never a sentence.
bool wantsVersion(const std::vector<std::string>& argv);

} // namespace bridge
} // namespace tuxblox
