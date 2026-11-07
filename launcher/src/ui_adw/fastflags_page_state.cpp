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

#include "fastflags_page_state.h"

#include <algorithm>
#include <cctype>

namespace tuxblox {

namespace {

std::string lowered(const std::string& text) {
    std::string copy = text;
    std::transform(copy.begin(), copy.end(), copy.begin(),
                    [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
    return copy;
}

} // namespace

std::vector<size_t> visibleFlagIndices(const std::vector<FastFlag>& flags, const std::string& term) {
    std::vector<size_t> visible;
    const std::string wanted = lowered(term);
    for (size_t at = 0; at < flags.size(); ++at) {
        if (!wanted.empty() && lowered(flags[at].name).find(wanted) == std::string::npos) continue;
        visible.push_back(at);
    }
    return visible;
}

size_t flagCount(const std::vector<FastFlag>& flags) {
    return flags.size();
}

} // namespace tuxblox
