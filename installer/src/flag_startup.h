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
#include <string>
#include "FlagManager.h"

namespace tuxblox {

// Loads the flags for an installer run. The channel is the one the run was told to use
// (--channel), not whatever the install currently records: an update that is moving an
// install to canary should read canary's flags while it does it.
//
// A run with no recorded release -- which is every fresh install, because resolving the
// release is what it is about to do -- asks nobody.
void initInstallerFlags(const std::string& installDir, const std::string& channel,
                        FlagFetch fetch = {});

// The same, against a published-or-not of the caller's choosing rather than this binary's own.
// Exposed so the gate that decides whether anything is asked of the server can be tested, which it
// otherwise could not be: the real answer is compiled into whichever binary is asking.
void initInstallerFlagsFor(const std::string& installDir, const std::string& channel,
                           bool published, FlagFetch fetch = {});

} // namespace tuxblox
