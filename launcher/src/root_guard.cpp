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

#include "root_guard.h"
#include <cstdio>
#include <cstring>

namespace tuxblox {

namespace {

bool AllowRoot = false;

const char* const RootWarning =
    "WARNING: The current user is root, it is highly recommended to launch TuxBlox as a normal user unless there is a specific reason why";

const char* const RootRefusal =
    "TuxBlox: refusing to run as root. Re-run as a normal user, or pass --allow-root if you have a specific reason.";

} // namespace

bool takeAllowRootFlag(int& argc, char** argv) {
    bool found = false;
    for (int i = 1; i < argc;) {
        if (std::strcmp(argv[i], "--allow-root") != 0) {
            i++;
            continue;
        }
        found = true;
        for (int j = i; j < argc - 1; j++) argv[j] = argv[j + 1];
        argv[--argc] = nullptr;
    }
    return found;
}

bool rootRunAllowed(bool allowRoot, uid_t effectiveUid) {
    if (effectiveUid != 0) return true;
    if (!allowRoot) {
        fprintf(stderr, "%s\n", RootRefusal);
        return false;
    }
    printf("%s\n", RootWarning);
    fflush(stdout);
    return true;
}

void setAllowRoot(bool allowed) { AllowRoot = allowed; }

bool allowRoot() { return AllowRoot; }

} // namespace tuxblox
