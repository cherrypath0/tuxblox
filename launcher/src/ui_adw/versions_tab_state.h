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
#include "versions_manifest.h"
#include "app.h"

namespace tuxblox {

// Whether the install sheet offers a channel to type. Latest and Custom are the two
// where it is a real question; Next is defined by Roblox's unreleased channel and
// Previous is only served for the channels TuxBlox records, so neither has one.
bool channelBoxVisibleFor(VersionSelectMode mode);

// Whether the sheet shows the notice that a build has not been released yet.
bool nextWarningVisibleFor(VersionSelectMode mode);

// True if `hash` is actually installed -- the rule VersionsTab's per-row
// delete button enables against. The version in use is deletable too: with
// one version installed it is always the active one, so guarding it left
// Delete permanently greyed out with nothing explaining why. Deleting it
// simply leaves the prefix without that version, and the next launch
// installs Roblox again.
// Kept apart from the Versions page so it can be tested without a display.
bool canDeleteVersion(const AppVersions& av, const std::string& hash);

// The channel as it is sent: lowercase, without spaces around it
std::string normalizedChannel(const std::string& typed);

// A version hash as it is sent. Copying one out of a log or a web page brings whitespace with it, and the URL that builds is refused.
std::string trimmedHash(const std::string& typed);

// What a version's row says under its hash: the version Roblox reports and the
// channel, separated by a dot. Deliberately carries neither the install date nor
// the size on disk -- the size would need a recursive walk of a 1.3 GB directory
// per version, and the date earns nothing. installedAt is still stored, just not shown.
std::string versionRowMeta(const InstalledVersion& version);

// Whether this is the version a launch would start.
bool versionIsActive(const AppVersions& versions, const std::string& hash);

} // namespace tuxblox
