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

namespace tuxblox {

// True if `hash` is in `av.installed` AND is not `av.activeHash` -- the
// exact rule VersionsTab's per-card delete button enables/disables against.
// Pulled out of VersionsTab itself so it's testable without a QApplication.
bool canDeleteVersion(const AppVersions& av, const std::string& hash);

} // namespace tuxblox
