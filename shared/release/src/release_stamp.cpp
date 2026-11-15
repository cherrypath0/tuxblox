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

// The field is 64 bytes and the marker takes the first 25, so a release and a channel have 39 to
// sit in -- plenty for four numbers and the longest channel name, with room to spare.
constexpr std::size_t kFieldSize = 64;
constexpr std::size_t kMarkerSize = 25;

} // namespace

// Written after the build, by the maintainer's release tooling: it finds this field by its marker
// and overwrites everything after it with the release. The marker must appear exactly once in the
// binary, which is why it is spelled out here and nowhere else.
//
// It is volatile because it would otherwise be a constant the compiler can see all of: reads would
// be folded into the empty value it is compiled with, and the number written into the file would
// never be the number the program reports. extern, so it survives as a symbol of its own.
extern const volatile char kReleaseStampField[kFieldSize];
#ifdef TUXBLOX_RELEASE
// The build bakes in the release the root VERSION file declares, so a binary that was never staged
// still answers with one. Staging overwrites it either way: it finds the marker and writes past it,
// whatever follows. Too long a release fails here rather than being silently cut to fit.
const volatile char kReleaseStampField[kFieldSize] = "TUXBLOX_RELEASE_STAMP_V1:" TUXBLOX_RELEASE;
#else
const volatile char kReleaseStampField[kFieldSize] = "TUXBLOX_RELEASE_STAMP_V1:";
#endif

namespace {

// The release and the channel are written one after the other, each ending in a zero byte, so
// anything that reads only the first of them stops where it always did.
std::string fieldEntry(std::size_t which) {
    std::string out;
    std::size_t at = kMarkerSize;
    for (std::size_t entry = 0; entry <= which; ++entry) {
        out.clear();
        for (; at < kFieldSize; ++at) {
            const char c = kReleaseStampField[at];
            if (c == '\0') break;
            out.push_back(c);
        }
        ++at; // past the zero byte that ended it
    }
    return out;
}

} // namespace

const std::string& releaseVersion() {
    // Read once: the file is stamped before anybody runs it, so the field cannot change underneath
    // a running program, and a volatile read is not something to repeat on every caller's behalf.
    static const std::string value = fieldEntry(0);
    return value;
}

const std::string& releaseChannel() {
    static const std::string value = fieldEntry(1);
    return value;
}

std::string reportedVersion(const std::string& fallback) {
    const std::string& release = releaseVersion();
    return release.empty() ? fallback : release;
}

} // namespace tuxblox
