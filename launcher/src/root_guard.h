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
#include <sys/types.h>

namespace tuxblox {

// Removes every "--allow-root" from argv and reports whether one was there.
// The launcher's other arguments are positional -- a roblox: link arrives as
// argv[1] -- so the flag has to be taken out before anything else reads them.
bool takeAllowRootFlag(int& argc, char** argv);

// Says whether TuxBlox may carry on, warning or refusing on the console first.
// Takes the user id rather than calling geteuid() so it can be tested.
bool rootRunAllowed(bool allowRoot, uid_t effectiveUid);

// Remembered for the whole process, because the two places that start the
// compatibility layer are a long way from the arguments.
void setAllowRoot(bool allowed);
bool allowRoot();

} // namespace tuxblox
