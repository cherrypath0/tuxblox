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

namespace tuxblox {

// True if any Roblox process is currently live inside `prefixDir` -- the
// WINEPREFIX, i.e. installDir + "/runtime/pfx", NOT installDir + "/runtime".
//
// Mirrors prefix_has_session_holder() in ProtonSource/proton.py (line 554) and
// must stay in sync with it, including the image allowlist. The launcher needs
// its own copy because it has to answer this BEFORE deciding which Proton verb
// to invoke -- see ProcessLauncher::launch().
bool prefixHasSessionHolder(const std::string& prefixDir);

// Testable seams, exposed for tests rather than for callers.

// Extracts the lowercased image basename from a wine process's
// /proc/<pid>/cmdline first token, or "" if it isn't a wine process.
std::string wineImageNameFromCmdline(const std::string& firstCmdlineToken);

// prefixHasSessionHolder() against an arbitrary /proc-shaped root.
bool prefixHasSessionHolderIn(const std::string& procRoot, const std::string& prefixDir);

} // namespace tuxblox
