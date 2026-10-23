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
#include <utility>
#include <vector>

namespace tuxblox {

// What the About screen says about this install.
//
// The release is the one number a person is shown, because it is the one that identifies the
// install: a component version appears in several releases, so no binary can name the release it
// belongs to. The component versions are real and worth showing, but behind an expander rather
// than beside it -- two numbers side by side read as an install at odds with itself.
struct AboutVersions {
    std::string release;  // empty for an install made before the release file existed
    std::string channel;  // empty with the release
    // {display name, version}, always one row per component in the layout's order. A component
    // that is not installed is named as such rather than left out, so an absence is visible.
    std::vector<std::pair<std::string, std::string>> components;
};

AboutVersions aboutVersions(const std::string& installDir, const std::string& launcherVersion);

} // namespace tuxblox
