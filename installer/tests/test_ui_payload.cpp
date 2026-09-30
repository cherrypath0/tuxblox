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
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// Creates a trailer with the given digest for testing.
static tuxblox::UiPayloadTrailer makeTrailerWithDigest(const std::string& digest, uint64_t offset = 0, uint64_t size = 16) {
    tuxblox::UiPayloadTrailer t;
    t.offset = offset;
    t.size = size;
    t.sha256 = digest;
    t.format = 1;
    return t;
}

// Writes `body` followed by a trailer describing a payload at [offset, offset+size).
static void writeBinary(const fs::path& path, const std::string& body,
                        uint64_t offset, uint64_t size, const std::string& digest) {
    tuxblox::UiPayloadTrailer t = makeTrailerWithDigest(digest, offset, size);
    auto encoded = tuxblox::encodeUiPayloadTrailer(t);
    assert(encoded.has_value());
    std::ofstream out(path, std::ios::binary);
    out << body << *encoded;
}

int main() {
    using namespace tuxblox;

    const std::string digest(64, 'a');
    fs::path work = fs::temp_directory_path() / "tuxblox_test_ui_payload";
    fs::remove_all(work);
    fs::create_directories(work);

    // A trailer is a fixed size, so the reader can always find it by seeking from the end.
    {
        UiPayloadTrailer t = makeTrailerWithDigest(digest, 4096, 128);
        auto encoded = encodeUiPayloadTrailer(t);
        assert(encoded.has_value());
        assert(encoded->size() == kUiPayloadTrailerSize);
    }

    // Round trip: what was encoded is what comes back.
    {
        const std::string body(4096 + 128, 'x');
        writeBinary(work / "ok", body, 4096, 128, digest);
        auto got = readUiPayloadTrailer((work / "ok").string());
        assert(got.has_value());
        assert(got->offset == 4096);
        assert(got->size == 128);
        assert(got->sha256 == digest);
        assert(got->format == 1);
    }

    // No payload at all is the ordinary case for a binary built without one, not an error.
    {
        std::ofstream out(work / "bare", std::ios::binary);
        out << std::string(1024, 'x');
        out.close();
        assert(!readUiPayloadTrailer((work / "bare").string()).has_value());
    }

    // Shorter than a trailer.
    {
        std::ofstream out(work / "tiny", std::ios::binary);
        out << "short";
        out.close();
        assert(!readUiPayloadTrailer((work / "tiny").string()).has_value());
    }

    // A recorded extent that runs past the end of the file describes a payload that is not there.
    {
        const std::string body(512, 'x');
        writeBinary(work / "overrun", body, 256, 1 << 20, digest);
        assert(!readUiPayloadTrailer((work / "overrun").string()).has_value());
    }

    // An extent overlapping the trailer itself is equally impossible.
    {
        const std::string body(512, 'x');
        writeBinary(work / "overlap", body, 500, 60, digest);
        assert(!readUiPayloadTrailer((work / "overlap").string()).has_value());
    }

    // A format this build does not understand must read as absent rather than be guessed at.
    {
        UiPayloadTrailer t = makeTrailerWithDigest(digest);
        t.format = 99;
        auto encoded = encodeUiPayloadTrailer(t);
        assert(encoded.has_value());
        std::ofstream out(work / "future", std::ios::binary);
        out << std::string(64, 'x') << *encoded;
        out.close();
        assert(!readUiPayloadTrailer((work / "future").string()).has_value());
    }

    // A missing file is absent, not a throw.
    assert(!readUiPayloadTrailer((work / "nope").string()).has_value());

    // An offset+size that wraps uint64 is impossible, even though the addition wraps to zero.
    {
        UiPayloadTrailer t = makeTrailerWithDigest(digest, 100, 0xFFFFFFFFFFFFFF9CULL);
        auto encoded = encodeUiPayloadTrailer(t);
        assert(encoded.has_value());
        std::ofstream out(work / "wrap", std::ios::binary);
        out << std::string(200, 'x') << *encoded;
        out.close();
        assert(!readUiPayloadTrailer((work / "wrap").string()).has_value());
    }

    // Malformed digests must be rejected, not silently padded or truncated.
    {
        assert(encodeUiPayloadTrailer(makeTrailerWithDigest(digest)).has_value());
        assert(encodeUiPayloadTrailer(makeTrailerWithDigest(digest))->size() == kUiPayloadTrailerSize);
        assert(!encodeUiPayloadTrailer(makeTrailerWithDigest("")).has_value());
        assert(!encodeUiPayloadTrailer(makeTrailerWithDigest(std::string(63, 'a'))).has_value());
        assert(!encodeUiPayloadTrailer(makeTrailerWithDigest(std::string(65, 'a'))).has_value());
        assert(!encodeUiPayloadTrailer(makeTrailerWithDigest(std::string(64, 'z'))).has_value());
        assert(encodeUiPayloadTrailer(makeTrailerWithDigest(std::string(64, 'A'))).has_value());
    }

    fs::remove_all(work);
    printf("ui_payload: all tests passed\n");
    return 0;
}
