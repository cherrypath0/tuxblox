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

#include "container_env.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    using namespace tuxblox;

    auto envWith = [](const char* value) {
        return [value](const char* name) -> const char* {
            return std::strcmp(name, "CONTAINER_ID") == 0 ? value : nullptr;
        };
    };

    assert(isInsideDistrobox(envWith(nullptr), true) == false);   // no CONTAINER_ID at all
    assert(isInsideDistrobox(envWith(""), true) == false);        // empty CONTAINER_ID
    assert(isInsideDistrobox(envWith("my-box"), false) == false); // CONTAINER_ID set, no /run/.containerenv
    assert(isInsideDistrobox(envWith("my-box"), true) == true);   // both signals present

    printf("container_env: all tests passed\n");
    return 0;
}
