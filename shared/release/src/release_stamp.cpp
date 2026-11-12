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

#include "release_stamp.h"

#include <cstddef>

namespace tuxblox {
namespace {

// The field is 64 bytes and the marker takes the first 25, so a release has 39 to sit in -- plenty
// for four numbers, and room for whatever a release version becomes later.
constexpr std::size_t kFieldSize = 64;
constexpr std::size_t kMarkerSize = 25;

} // namespace

// Written after the build, by shared/tools/stamp_release.py: it finds this field by its marker and
// overwrites everything after it with the release. The marker must appear exactly once in the
// binary, which is why it is spelled out here and nowhere else.
//
// It is volatile because it would otherwise be a constant the compiler can see all of: reads would
// be folded into the empty value it is compiled with, and the number written into the file would
// never be the number the program reports. extern, so it survives as a symbol of its own.
extern const volatile char kReleaseStampField[kFieldSize];
const volatile char kReleaseStampField[kFieldSize] = "TUXBLOX_RELEASE_STAMP_V1:";

const std::string& releaseVersion() {
    // Read once: the file is stamped before anybody runs it, so the field cannot change underneath
    // a running program, and a volatile read is not something to repeat on every caller's behalf.
    static const std::string value = [] {
        std::string out;
        for (std::size_t i = kMarkerSize; i < kFieldSize; ++i) {
            const char c = kReleaseStampField[i];
            if (c == '\0') break;
            out.push_back(c);
        }
        return out;
    }();
    return value;
}

std::string reportedVersion(const std::string& fallback) {
    const std::string& release = releaseVersion();
    return release.empty() ? fallback : release;
}

} // namespace tuxblox
