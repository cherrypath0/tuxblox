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

// Finds the certificates a secure download is checked against. TuxBlox brings its own secure
// transport, which has no list of its own on a user's machine, so the list has to be looked up.

#ifndef TUXBLOX_CA_BUNDLE_H
#define TUXBLOX_CA_BUNDLE_H

#include <string>

#include <curl/curl.h>

namespace tuxblox {

// The list to check against, or empty when nothing on this machine was usable and the copy built into TuxBlox should be used.
std::string caBundlePath();

// The copy built into TuxBlox, used only on a machine that has none of its own.
std::string caBundleEmbedded();

// Points a download at whichever of the two applies.
void applyCaBundle(CURL *pCurl);

} // namespace tuxblox

#endif // TUXBLOX_CA_BUNDLE_H
