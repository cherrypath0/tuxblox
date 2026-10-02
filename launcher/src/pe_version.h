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

// The version a Windows program reports for itself, e.g. "0.740.0.7400927" for
// RobloxPlayerBeta.exe. Empty when the file is missing, is not a Windows
// program, or carries no version of its own -- every one of which is an
// ordinary outcome here, not an error, since the caller falls back to the
// version hash.
//
// Roblox's build number does not fit the four 16-bit fields of the numeric
// version a Windows program also carries, so the text is the only accurate
// source and this reads that.
std::string peFileVersion(const std::string& path);

// "0, 740, 0, 7400927" -> "0.740.0.7400927". Empty unless the input really is
// four numbers, so a version resource holding something else is refused rather
// than shown half-formatted.
std::string normalizeVersionString(const std::string& raw);

// The normalized FileVersion out of a version resource's own bytes. Separate
// from peFileVersion() because finding those bytes and reading them are
// different jobs, and this one is where the format's quirks live.
std::string fileVersionFromResource(const std::string& resourceBytes);

} // namespace tuxblox
