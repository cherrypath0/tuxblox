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

#include "pe_version.h"
#include <cstdint>
#include <fstream>
#include <vector>

namespace tuxblox {

namespace {

constexpr uint16_t kVersionResourceType = 16;
// A version resource holds a few hundred bytes of text. Anything far larger is not one, and reading it would be the only unbounded read here.
constexpr uint32_t kMaxResourceSize = 1u << 20;

uint16_t readU16(const std::string& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return static_cast<uint16_t>(static_cast<unsigned char>(data[offset]) |
                                  (static_cast<unsigned char>(data[offset + 1]) << 8));
}

uint32_t readU32(const std::string& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(static_cast<unsigned char>(data[offset + i])) << (i * 8);
    }
    return value;
}

// One section of the loaded image, enough of it to turn an address back into a place in the file.
struct Section {
    uint32_t rva = 0;
    uint32_t virtualSize = 0;
    uint32_t rawSize = 0;
    uint32_t rawOffset = 0;
};

class PeReader {
public:
    explicit PeReader(const std::string& path) : file_(path, std::ios::binary) {}

    bool opened() const { return file_.is_open(); }

    // Empty unless the whole requested range was there, so every caller can treat a short file as "no answer" without checking twice.
    std::string read(uint64_t offset, size_t size) {
        if (size == 0 || size > kMaxResourceSize) return "";
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(offset));
        if (!file_) return "";
        std::string out(size, '\0');
        file_.read(out.data(), static_cast<std::streamsize>(size));
        return static_cast<size_t>(file_.gcount()) == size ? out : std::string();
    }

private:
    std::ifstream file_;
};

uint64_t fileOffsetForRva(const std::vector<Section>& sections, uint32_t rva) {
    for (const Section& section : sections) {
        const uint32_t span = section.virtualSize > section.rawSize ? section.virtualSize : section.rawSize;
        if (span == 0) continue;
        if (rva >= section.rva && rva - section.rva < span) {
            return static_cast<uint64_t>(section.rawOffset) + (rva - section.rva);
        }
    }
    return 0;
}

// Where the resource directory lives in the file, and the sections needed to resolve addresses inside it.
bool locateResourceDirectory(PeReader& reader, uint32_t& outRva, std::vector<Section>& outSections) {
    const std::string dos = reader.read(0, 0x40);
    if (dos.size() < 0x40 || dos[0] != 'M' || dos[1] != 'Z') return false;

    const uint32_t peOffset = readU32(dos, 0x3C);
    const std::string coff = reader.read(peOffset, 24);
    if (coff.size() < 24) return false;
    if (coff[0] != 'P' || coff[1] != 'E' || coff[2] != '\0' || coff[3] != '\0') return false;

    const uint16_t sectionCount = readU16(coff, 6);
    const uint16_t optionalSize = readU16(coff, 20);
    if (sectionCount == 0 || optionalSize < 2) return false;

    const std::string optional = reader.read(static_cast<uint64_t>(peOffset) + 24, optionalSize);
    if (optional.size() < optionalSize) return false;

    // The 64-bit header widens five fields, which is the only reason the directories sit elsewhere.
    const uint16_t magic = readU16(optional, 0);
    size_t directoriesAt;
    if (magic == 0x10B) directoriesAt = 96;
    else if (magic == 0x20B) directoriesAt = 112;
    else return false;

    const uint32_t directoryCount = readU32(optional, directoriesAt - 4);
    if (directoryCount < 3) return false;
    const size_t resourceAt = directoriesAt + 2 * 8;
    if (resourceAt + 8 > optional.size()) return false;
    outRva = readU32(optional, resourceAt);
    if (outRva == 0) return false;

    const std::string table =
        reader.read(static_cast<uint64_t>(peOffset) + 24 + optionalSize, static_cast<size_t>(sectionCount) * 40);
    if (table.size() < static_cast<size_t>(sectionCount) * 40) return false;
    for (uint16_t i = 0; i < sectionCount; ++i) {
        const size_t at = static_cast<size_t>(i) * 40;
        Section section;
        section.virtualSize = readU32(table, at + 8);
        section.rva = readU32(table, at + 12);
        section.rawSize = readU32(table, at + 16);
        section.rawOffset = readU32(table, at + 20);
        outSections.push_back(section);
    }
    return true;
}

// One step down the resource tree. `wantedId` picks an entry by id; without it the first entry wins,
// which is what the name and language levels need -- any language's copy carries the same version.
bool resourceChild(PeReader& reader, uint64_t directoryBase, uint32_t directoryOffset, bool matchId,
                   uint32_t wantedId, uint32_t& outOffset, bool& outIsDirectory) {
    const std::string header = reader.read(directoryBase + directoryOffset, 16);
    if (header.size() < 16) return false;
    const uint32_t entries =
        static_cast<uint32_t>(readU16(header, 12)) + static_cast<uint32_t>(readU16(header, 14));
    if (entries == 0 || entries > 4096) return false;

    const std::string list = reader.read(directoryBase + directoryOffset + 16, entries * 8);
    if (list.size() < entries * 8) return false;
    for (uint32_t i = 0; i < entries; ++i) {
        const uint32_t name = readU32(list, i * 8);
        const uint32_t offset = readU32(list, i * 8 + 4);
        // A name here is a string rather than an id, so it can never be the id being looked for.
        if (matchId && ((name & 0x80000000u) != 0 || name != wantedId)) continue;
        outOffset = offset & 0x7FFFFFFFu;
        outIsDirectory = (offset & 0x80000000u) != 0;
        return true;
    }
    return false;
}

} // namespace

std::string normalizeVersionString(const std::string& raw) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : raw) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (c == ',' || c == '.') {
            parts.push_back(current);
            current.clear();
            continue;
        }
        if (c < '0' || c > '9') return "";
        current.push_back(c);
    }
    parts.push_back(current);

    if (parts.size() != 4) return "";
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i].empty()) return "";
        if (i != 0) out.push_back('.');
        out += parts[i];
    }
    return out;
}

std::string fileVersionFromResource(const std::string& resourceBytes) {
    std::string needle;
    for (char c : std::string("FileVersion")) {
        needle.push_back(c);
        needle.push_back('\0');
    }
    needle.push_back('\0');
    needle.push_back('\0');

    size_t at = 0;
    while ((at = resourceBytes.find(needle, at)) != std::string::npos) {
        const size_t keyAt = at;
        at += 2;
        if (keyAt % 2 != 0) continue;
        // The two bytes before a key say what kind of entry it is, so they are never a letter -- which is what tells this key apart from a longer one ending in the same word, such as AssemblyFileVersion.
        if (keyAt < 2 || readU16(resourceBytes, keyAt - 2) >= 0x20) continue;

        // The value is aligned to four bytes from the start of the resource, not from the end of the key.
        size_t valueAt = (keyAt + needle.size() + 3) & ~static_cast<size_t>(3);
        std::string value;
        bool readable = true;
        while (valueAt + 2 <= resourceBytes.size()) {
            const uint16_t ch = readU16(resourceBytes, valueAt);
            if (ch == 0) break;
            if (ch > 0x7F) {
                readable = false;
                break;
            }
            value.push_back(static_cast<char>(ch));
            valueAt += 2;
        }
        if (!readable) continue;
        const std::string normalized = normalizeVersionString(value);
        if (!normalized.empty()) return normalized;
    }
    return "";
}

std::string peFileVersion(const std::string& path) {
    PeReader reader(path);
    if (!reader.opened()) return "";

    uint32_t resourceRva = 0;
    std::vector<Section> sections;
    if (!locateResourceDirectory(reader, resourceRva, sections)) return "";
    const uint64_t resourceBase = fileOffsetForRva(sections, resourceRva);
    if (resourceBase == 0) return "";

    uint32_t offset = 0;
    bool isDirectory = false;
    if (!resourceChild(reader, resourceBase, 0, true, kVersionResourceType, offset, isDirectory)) return "";
    if (!isDirectory) return "";
    if (!resourceChild(reader, resourceBase, offset, false, 0, offset, isDirectory)) return "";
    if (!isDirectory) return "";
    if (!resourceChild(reader, resourceBase, offset, false, 0, offset, isDirectory)) return "";
    if (isDirectory) return "";

    const std::string entry = reader.read(resourceBase + offset, 16);
    if (entry.size() < 16) return "";
    const uint64_t blobAt = fileOffsetForRva(sections, readU32(entry, 0));
    const uint32_t blobSize = readU32(entry, 4);
    if (blobAt == 0 || blobSize == 0) return "";

    return fileVersionFromResource(reader.read(blobAt, blobSize > kMaxResourceSize ? kMaxResourceSize : blobSize));
}

} // namespace tuxblox
