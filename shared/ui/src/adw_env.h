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
#include <vector>

namespace tuxblox {

// Points fontconfig and GLib at the stack unpacked at `stackRoot`, remembering what both variables held. A variable is only changed when the file it should name is readable, and a second call keeps the first call's memory.
void useBundledEnvironment(const std::string &stackRoot);

// Puts FONTCONFIG_FILE and GSETTINGS_SCHEMA_DIR back exactly as useBundledEnvironment() found them, including unset. Does nothing if it was never called.
void restoreBundledEnvironment();

// A copy of environ with FONTCONFIG_FILE and GSETTINGS_SCHEMA_DIR put back as useBundledEnvironment() found them, for starting another program. The same as environ when nothing is bundled.
std::vector<std::string> unbundledEnvironment();

} // namespace tuxblox
