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

#include "bootstrap/command.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace tuxblox::bootstrap;

    assert(isCommandFlag("--update-roblox"));
    assert(isCommandFlag("--install-roblox"));
    assert(isCommandFlag("--preview-rbxinstaller"));

    // The old names are gone, not aliased: a stale caller must fail loudly.
    assert(!isCommandFlag("--update"));
    assert(!isCommandFlag("--install"));
    assert(!isCommandFlag("--preview"));

    // Nothing the launcher owns may be captured by this dispatch.
    assert(!isCommandFlag("--launch-player"));
    assert(!isCommandFlag("--watch-launch"));
    assert(!isCommandFlag("--message-box"));
    assert(!isCommandFlag("--version"));
    assert(!isCommandFlag(""));

    printf("bootstrap_dispatch: all tests passed\n");
    return 0;
}
