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
#include <cstdint>
#include <optional>
#include <string>

namespace tuxblox {

// Describes the UI stack appended after this binary's ELF image.
struct UiPayloadTrailer {
    uint64_t offset = 0;      // payload start, from the beginning of the file
    uint64_t size = 0;        // payload length in bytes
    std::string sha256;       // hex (uppercase or lowercase), 64 characters
    uint32_t format = 1;      // bumped only if this record's own layout changes
};

// magic(10) + format(4) + offset(8) + size(8) + digest(32 raw bytes)
constexpr size_t kUiPayloadTrailerSize = 62;

// The only format this build understands.
constexpr uint32_t kUiPayloadFormat = 1;

// Encodes a trailer as exactly kUiPayloadTrailerSize bytes, little-endian. Returns nullopt if the digest is not exactly 64 hex characters.
std::optional<std::string> encodeUiPayloadTrailer(const UiPayloadTrailer& trailer);

// Reads the trailer from the end of `binaryPath`. Absent means there is no usable payload -- too short, wrong magic, unknown format, or an extent that does not fit inside the file -- all of which are ordinary conditions rather than failures to report.
std::optional<UiPayloadTrailer> readUiPayloadTrailer(const std::string& binaryPath);

} // namespace tuxblox
