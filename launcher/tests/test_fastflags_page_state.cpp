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

#include "ui_adw/fastflags_page_state.h"

#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    using namespace tuxblox;

    // An empty term hides nothing.
    {
        const std::vector<FastFlag> flags{{"FFlagA", "1"}, {"DFIntB", "2"}};
        assert(visibleFlagIndices(flags, "").size() == 2);
        assert(flagCount(flags) == 2);
    }

    // Case-insensitive, and on the name only -- searching a value finds nothing.
    {
        const std::vector<FastFlag> flags{{"FFlagDebugGraphicsPreferVulkan", "True"},
                                          {"DFIntTaskSchedulerTargetFps", "144"}};
        assert(visibleFlagIndices(flags, "vulkan").size() == 1);
        assert(visibleFlagIndices(flags, "VULKAN").size() == 1);
        assert(visibleFlagIndices(flags, "VuLkAn").size() == 1);
        assert(visibleFlagIndices(flags, "144").empty());
    }

    // The indices are into the original list, so a row the term hides is still
    // addressable -- filtering must never drop a flag from what gets saved.
    {
        const std::vector<FastFlag> flags{{"A", "1"}, {"Bmatch", "2"}, {"C", "3"}};
        const std::vector<size_t> visible = visibleFlagIndices(flags, "match");
        assert(visible.size() == 1);
        assert(visible[0] == 1);
        assert(flagCount(flags) == 3);
    }

    // A term nothing matches hides every row rather than falling back to all of them.
    {
        const std::vector<FastFlag> flags{{"A", "1"}, {"B", "2"}};
        assert(visibleFlagIndices(flags, "zzz").empty());
    }

    // No flags at all is not a special case.
    {
        assert(visibleFlagIndices({}, "").empty());
        assert(flagCount({}) == 0);
    }

    std::printf("fastflags_page_state: all tests passed\n");
    return 0;
}
