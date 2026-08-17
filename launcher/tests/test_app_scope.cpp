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

#include "app_scope.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace tuxblox;

    // '-' is the path separator inside systemd unit names, so it must be
    // escaped or "tuxblox-launcher" can't be recovered from the unit name.
    // This is the same escaping KDE's own units use, e.g.
    // app-org.kde.plasma\x2dsystemmonitor@<hash>.service.
    assert(escapeUnitName("tuxblox-launcher") == "tuxblox\\x2dlauncher");
    assert(escapeUnitName("tuxblox") == "tuxblox");
    assert(escapeUnitName("a-b-c") == "a\\x2db\\x2dc");
    assert(escapeUnitName("") == "");
    // '.' is not special in unit names and must survive untouched, or ids
    // like org.kde.foo would be mangled.
    assert(escapeUnitName("org.kde.foo") == "org.kde.foo");

    assert(appScopeUnitName("tuxblox-launcher", 1234) == "app-tuxblox\\x2dlauncher-1234.scope");

    // A path-named unit -- what a raw-binary launch produces -- must NOT
    // count as already-scoped, or the fix would no-op in exactly the case it
    // exists for.
    const std::string pathScoped =
        "0::/user.slice/user-1000.slice/user@1000.service/app.slice/"
        "app-\\x2fhome\\x2fcherry\\x2f.tuxblox\\x2fTuxBloxLauncher@abc123.service\n";
    assert(alreadyInAppScope(pathScoped, "tuxblox-launcher") == false);

    // Our own scope must be recognised, so we don't stack a second one.
    const std::string ourScope =
        "0::/user.slice/user-1000.slice/user@1000.service/app.slice/"
        "app-tuxblox\\x2dlauncher-4321.scope\n";
    assert(alreadyInAppScope(ourScope, "tuxblox-launcher") == true);

    // Inherited from the GUI by the --watch-launch helper: same scope, a pid
    // suffix that is not this process's. Must still count as already-scoped,
    // which is why the check ignores the suffix.
    const std::string inherited =
        "0::/user.slice/user-1000.slice/user@1000.service/app.slice/"
        "app-tuxblox\\x2dlauncher-999.scope\n";
    assert(alreadyInAppScope(inherited, "tuxblox-launcher") == true);

    // A terminal's scope is somebody else's app -- not ours.
    const std::string alacritty =
        "0::/user.slice/user-1000.slice/user@1000.service/app.slice/"
        "app-Alacritty@9692d408.service\n";
    assert(alreadyInAppScope(alacritty, "tuxblox-launcher") == false);

    assert(alreadyInAppScope("", "tuxblox-launcher") == false);

    printf("app_scope: all tests passed\n");
    return 0;
}
