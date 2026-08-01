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
#include <cstddef>
#include <string>

namespace tuxblox {

// Returns the lowercase hex-encoded SHA-256 digest of the file at path.
// Throws std::runtime_error if the file cannot be opened or hashing fails.
std::string sha256File(const std::string& path);

// Returns the lowercase hex-encoded SHA-256 digest of an in-memory buffer.
std::string sha256Bytes(const unsigned char* data, size_t len);

} // namespace tuxblox
