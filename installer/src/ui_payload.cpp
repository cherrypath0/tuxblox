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

#include "ui_payload.h"
#include <cstring>
#include <fstream>

namespace tuxblox {

namespace {
constexpr char kMagic[10] = {'T','U','X','B','L','O','X','U','I','\0'};

void putU32(std::string& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
}

void putU64(std::string& out, uint64_t value) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
}

uint32_t getU32(const unsigned char* p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i);
    return v;
}

uint64_t getU64(const unsigned char* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(p[i]) << (8 * i);
    return v;
}

// The digest travels as 32 raw bytes and is presented as hex, so the trailer stays a fixed size.
std::string hexToRaw(const std::string& hex) {
    std::string raw;
    raw.reserve(32);
    for (size_t i = 0; i + 1 < hex.size() && raw.size() < 32; i += 2) {
        raw.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    raw.resize(32, '\0');
    return raw;
}

std::string rawToHex(const unsigned char* raw) {
    static const char* digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(64);
    for (int i = 0; i < 32; ++i) {
        hex.push_back(digits[raw[i] >> 4]);
        hex.push_back(digits[raw[i] & 0x0F]);
    }
    return hex;
}
} // namespace

std::string encodeUiPayloadTrailer(const UiPayloadTrailer& trailer) {
    std::string out;
    out.reserve(kUiPayloadTrailerSize);
    out.append(kMagic, sizeof(kMagic));
    putU32(out, trailer.format);
    putU64(out, trailer.offset);
    putU64(out, trailer.size);
    out.append(hexToRaw(trailer.sha256));
    return out;
}

std::optional<UiPayloadTrailer> readUiPayloadTrailer(const std::string& binaryPath) {
    std::ifstream in(binaryPath, std::ios::binary);
    if (!in) return std::nullopt;

    in.seekg(0, std::ios::end);
    const std::streamoff fileSize = in.tellg();
    if (fileSize < static_cast<std::streamoff>(kUiPayloadTrailerSize)) return std::nullopt;

    in.seekg(fileSize - static_cast<std::streamoff>(kUiPayloadTrailerSize), std::ios::beg);
    unsigned char buf[kUiPayloadTrailerSize];
    if (!in.read(reinterpret_cast<char*>(buf), kUiPayloadTrailerSize)) return std::nullopt;
    if (std::memcmp(buf, kMagic, sizeof(kMagic)) != 0) return std::nullopt;

    UiPayloadTrailer trailer;
    trailer.format = getU32(buf + 10);
    if (trailer.format != kUiPayloadFormat) return std::nullopt;
    trailer.offset = getU64(buf + 14);
    trailer.size = getU64(buf + 22);
    trailer.sha256 = rawToHex(buf + 30);

    // The payload has to lie wholly inside the file and stop before the trailer it is described by.
    const uint64_t trailerStart = static_cast<uint64_t>(fileSize) - kUiPayloadTrailerSize;
    if (trailer.size == 0) return std::nullopt;
    if (trailer.offset > trailerStart) return std::nullopt;
    if (trailer.offset + trailer.size > trailerStart) return std::nullopt;

    return trailer;
}

} // namespace tuxblox
