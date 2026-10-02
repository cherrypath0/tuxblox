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
// Builds Windows programs in memory for the tests that have to read one.
// Test-only: nothing here ships.

#pragma once
#include <cassert>
#include <cstdint>
#include <string>

namespace tuxblox_test {

inline void putU16(std::string& out, uint16_t value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
}

inline void putU32(std::string& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((value >> (i * 8)) & 0xFF));
}

// A UTF-16LE string with its terminator, which is how every key and value inside a version resource is stored.
inline void putWide(std::string& out, const std::string& ascii) {
    for (char c : ascii) putU16(out, static_cast<unsigned char>(c));
    putU16(out, 0);
}

inline void padTo4(std::string& out, size_t base) {
    while ((out.size() - base) % 4 != 0) out.push_back('\0');
}

// One "String" entry as a version resource stores it: a length header, the key, padding, then the value.
inline void putStringEntry(std::string& out, size_t base, const std::string& key, const std::string& value) {
    padTo4(out, base);
    putU16(out, 0);                                       // wLength, not read by the parser
    putU16(out, static_cast<uint16_t>(value.size() + 1)); // wValueLength, in characters
    putU16(out, 1);                                       // wType: text
    putWide(out, key);
    padTo4(out, base);
    putWide(out, value);
}

inline std::string versionResourceWith(const std::string& key, const std::string& value) {
    std::string blob;
    putStringEntry(blob, 0, key, value);
    return blob;
}

// A PE64 file holding `resource` as its only resource, reachable the way a real one is: through the
// optional header's resource data directory, then the three levels of the resource tree.
inline std::string minimalPe(const std::string& resource, uint16_t typeId = 16) {
    const uint32_t rsrcRva = 0x1000;
    const uint32_t headersSize = 0x200;

    // Three directories, one entry each, then the data entry, then the blob itself.
    const uint32_t dir2 = 16 + 8;
    const uint32_t dir3 = dir2 + 16 + 8;
    const uint32_t dataEntry = dir3 + 16 + 8;
    const uint32_t blobOffset = dataEntry + 16;

    std::string rsrc;
    auto directory = [&rsrc](uint16_t id, uint32_t offset, bool subdirectory) {
        putU32(rsrc, 0); // Characteristics
        putU32(rsrc, 0); // TimeDateStamp
        putU32(rsrc, 0); // Major and minor version
        putU16(rsrc, 0); // NumberOfNamedEntries
        putU16(rsrc, 1); // NumberOfIdEntries
        putU32(rsrc, id);
        putU32(rsrc, subdirectory ? (offset | 0x80000000u) : offset);
    };
    directory(typeId, dir2, true);
    directory(1, dir3, true);
    directory(1033, dataEntry, false);
    putU32(rsrc, rsrcRva + blobOffset);
    putU32(rsrc, static_cast<uint32_t>(resource.size()));
    putU32(rsrc, 0); // CodePage
    putU32(rsrc, 0); // Reserved
    assert(rsrc.size() == blobOffset);
    rsrc += resource;

    std::string pe;
    pe += "MZ";
    pe.resize(0x3C, '\0');
    putU32(pe, 0x80); // e_lfanew
    pe.resize(0x80, '\0');

    pe += "PE";
    putU16(pe, 0);
    putU16(pe, 0x8664); // Machine: x86-64
    putU16(pe, 1);      // NumberOfSections
    putU32(pe, 0);      // TimeDateStamp
    putU32(pe, 0);      // PointerToSymbolTable
    putU32(pe, 0);      // NumberOfSymbols
    putU16(pe, 240);    // SizeOfOptionalHeader
    putU16(pe, 0x22);   // Characteristics

    const size_t optionalStart = pe.size();
    putU16(pe, 0x20B); // PE32+
    pe.resize(optionalStart + 108, '\0');
    putU32(pe, 16); // NumberOfRvaAndSizes
    putU32(pe, 0);  // Export table
    putU32(pe, 0);
    putU32(pe, 0); // Import table
    putU32(pe, 0);
    putU32(pe, rsrcRva); // Resource table
    putU32(pe, static_cast<uint32_t>(rsrc.size()));
    pe.resize(optionalStart + 240, '\0');

    pe.append(".rsrc\0\0\0", 8);                    // Name, NUL-padded to its full width
    putU32(pe, static_cast<uint32_t>(rsrc.size())); // VirtualSize
    putU32(pe, rsrcRva);                            // VirtualAddress
    putU32(pe, static_cast<uint32_t>(rsrc.size())); // SizeOfRawData
    putU32(pe, headersSize);                        // PointerToRawData
    pe.resize(pe.size() + 16, '\0');

    pe.resize(headersSize, '\0');
    pe += rsrc;
    return pe;
}

// A Windows program that reports `version`, e.g. "0, 740, 0, 7400927".
inline std::string peReporting(const std::string& version) {
    return minimalPe(versionResourceWith("FileVersion", version));
}

} // namespace tuxblox_test
