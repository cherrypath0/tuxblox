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
#include <map>
#include <string>
#include <vector>

// What an update would change, worked out from the two release files rather than from a changelog.
//
// A release names every component's version, so the components that move are the difference between
// the release an install records and the one the channel names. That needs nothing fetched and
// nothing published alongside the release, and it cannot disagree with what is about to be
// downloaded, because it is read from the same file the download is driven by.
namespace tuxblox {

struct ComponentChange {
    std::string display;  // what a person calls it, e.g. "Runtime"
    std::string from;     // empty when the install records no version for it
    std::string to;
};

// In the layout's order, so two of these read the same way. A component the new release does not
// name is left out rather than reported as removed: releases gain and lose components, and this
// build cannot say what losing one it has never heard of would mean.
std::vector<ComponentChange> updateChanges(const std::map<std::string, std::string>& recorded,
                                            const std::map<std::string, std::string>& named);

// "Launcher 3.5.0 -> Launcher 3.6.0", or just "Launcher 3.6.0" when there is nothing to come from.
std::string changeLine(const ComponentChange& change);

} // namespace tuxblox
