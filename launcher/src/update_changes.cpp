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

#include "update_changes.h"

#include "release_file.h"

namespace tuxblox {

std::vector<ComponentChange> updateChanges(const std::map<std::string, std::string>& recorded,
                                            const std::map<std::string, std::string>& named) {
    std::vector<ComponentChange> changes;
    for (const std::string& name : componentNames()) {
        const auto to = named.find(name);
        if (to == named.end() || to->second.empty()) continue;
        const auto from = recorded.find(name);
        const std::string was = from == recorded.end() ? std::string() : from->second;
        if (was == to->second) continue;
        const auto layout = componentLayout(name);
        changes.push_back({layout ? layout->displayName : name, was, to->second});
    }
    return changes;
}

std::string changeLine(const ComponentChange& change) {
    if (change.from.empty()) return change.display + " " + change.to;
    return change.display + " " + change.from + " \xE2\x86\x92 " + change.display + " " + change.to;
}

} // namespace tuxblox
