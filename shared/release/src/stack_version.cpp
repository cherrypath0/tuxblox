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

// What the interface stack answers when it is asked what it is. The stack is libraries rather than
// a program, so there is nothing in it to run with --version and nothing for staging to stamp;
// this library is both. It carries the release stamp field like every other component, so staging
// writes the release and the channel into it exactly as it does into the programs.

#include "release_stamp.h"

#ifndef TUXBLOX_COMPONENT_VERSION
#define TUXBLOX_COMPONENT_VERSION "unknown"
#endif

extern "C" {

// The release this copy of the stack was built as, which staging overwrites with the one it resolves.
const char *tuxblox_release_version(void) {
    return tuxblox::releaseVersion().c_str();
}

// The stack's own version, which moves when the libraries in it do, and is the number
// shared/ui/versions.env declares.
const char *tuxblox_component_version(void) {
    return TUXBLOX_COMPONENT_VERSION;
}

// The channel line 3 of the root VERSION file named, which staging overwrites with the real one.
const char *tuxblox_release_channel(void) {
    return tuxblox::releaseChannel().c_str();
}

} // extern "C"
