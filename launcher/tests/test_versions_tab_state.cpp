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

#include "versions_manifest.h"
#include "ui_qt/versions_tab_state.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace tuxblox;

    AppVersions av;
    av.installed = {{"version-a", "live", "t1"}, {"version-b", "live", "t2"}};
    av.activeHash = "version-a";

    assert(canDeleteVersion(av, "version-a"));   // active -- allowed, another version remains
    assert(canDeleteVersion(av, "version-b"));   // installed, not active -- allowed
    assert(!canDeleteVersion(av, "version-c"));  // not installed at all -- nothing to delete

    // The last remaining version stays deletable: the prefix simply ends up
    // with no Roblox in it, and the next launch installs one again.
    AppVersions only;
    only.installed = {{"version-a", "live", "t1"}};
    only.activeHash = "version-a";
    assert(canDeleteVersion(only, "version-a"));

    AppVersions none;
    assert(!canDeleteVersion(none, "version-a"));

    printf("versions_tab_state: all tests passed\n");
    return 0;
}
