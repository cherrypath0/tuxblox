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

namespace tuxblox::macclient {

// Checks an Apple-signed application bundle before it is allowed to run.
// Returns an empty string only when every check passed; any other string is
// the reason it was refused. Nothing is patched or rewritten -- the bundle is
// only read.
//
// This is deliberately fail-closed: an unrecognised signature layout, a
// malformed plist, a missing sealed file or an unexpected extra file all
// refuse the bundle. The checks are:
//   - the main executable's CodeDirectory page hashes;
//   - its CMS signature, chained to the pinned Apple Root CA, with the
//     Developer ID intermediate/leaf policy and the expected team;
//   - the identifier in the CodeDirectory;
//   - the Info.plist and CodeResources hashes in the special slots;
//   - every hash2/cdhash entry in CodeResources' files2 dictionary.
//
// The one file this runtime writes inside the bundle is
// Contents/MacOS/ClientSettings/ClientAppSettings.json (engine flags), which
// is outside the signature's seal and is explicitly allowed here.
std::string verifyAppSignature(const std::string& appDir,
                               const std::string& expectedIdentifier);

// The code-directory hash of a Mach-O file, as bytes. Used for the cdhash
// entries of nested code in CodeResources. Empty when the file cannot be
// parsed as a signed Mach-O.
std::string codeDirectoryHash(const std::string& path);

} // namespace tuxblox::macclient
