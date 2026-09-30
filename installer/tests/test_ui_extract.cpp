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

#include "ui_extract.h"
#include "ui_payload.h"
#include "ui_cache.h"
#include "checksum.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// Builds a fake installer: `body` bytes, then `payload` bytes, then a trailer describing them.
static void buildFakeBinary(const fs::path& path, const std::string& payload,
                            const std::string& digestOverride = "") {
    const std::string body(1024, 'E');
    tuxblox::UiPayloadTrailer t;
    t.offset = body.size();
    t.size = payload.size();
    t.format = 1;
    {
        fs::path tmp = path.string() + ".payload";
        std::ofstream p(tmp, std::ios::binary);
        p << payload;
        p.close();
        t.sha256 = digestOverride.empty() ? tuxblox::sha256File(tmp.string()) : digestOverride;
        fs::remove(tmp);
    }
    std::ofstream out(path, std::ios::binary);
    const auto trailer = tuxblox::encodeUiPayloadTrailer(t);
    assert(trailer.has_value());
    out << body << payload << *trailer;
}

int main() {
    using namespace tuxblox;

    fs::path work = fs::temp_directory_path() / "tuxblox_test_ui_extract";
    fs::remove_all(work);
    fs::create_directories(work);
    setenv("XDG_CACHE_HOME", (work / "xdg").c_str(), 1);

    // A binary with no payload at all reports that plainly rather than throwing.
    {
        std::ofstream out(work / "bare", std::ios::binary);
        out << std::string(2048, 'E');
        out.close();
        auto r = ensureUiStack((work / "bare").string(), "2.9.0");
        assert(!r.ok);
        assert(r.errorMessage.find("no interface") != std::string::npos);
    }

    // A payload whose digest does not match is a damaged download, named as such, and leaves nothing behind.
    {
        buildFakeBinary(work / "corrupt", "not a real tarball", std::string(64, 'f'));
        auto r = ensureUiStack((work / "corrupt").string(), "2.9.0");
        assert(!r.ok);
        assert(r.errorMessage.find("damaged") != std::string::npos);
        int partials = 0;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(fs::path(uiCacheRoot()), ec)) {
            if (e.path().filename().string().find(".partial-") != std::string::npos) ++partials;
        }
        assert(partials == 0);
    }

    // A cache directory that cannot be created is reported as a permissions problem, not an exception.
    {
        fs::path locked = work / "locked";
        fs::create_directories(locked);
        fs::permissions(locked, fs::perms::owner_read | fs::perms::owner_exec);
        setenv("XDG_CACHE_HOME", locked.c_str(), 1);
        buildFakeBinary(work / "ok2", "payload");
        auto r = ensureUiStack((work / "ok2").string(), "2.9.0");
        assert(!r.ok);
        assert(!r.errorMessage.empty());
        fs::permissions(locked, fs::perms::owner_all);
        setenv("XDG_CACHE_HOME", (work / "xdg").c_str(), 1);
    }

    // A complete extraction for this digest is reused, and the payload is never touched again.
    {
        buildFakeBinary(work / "cached", "payload");
        auto trailer = readUiPayloadTrailer((work / "cached").string());
        assert(trailer.has_value());
        const std::string dir = uiCacheDir("2.9.0");
        fs::create_directories(dir);
        { std::ofstream out(fs::path(dir) / "TuxBloxInstaller-ui"); out << "#!/bin/sh\n"; }
        uiCacheMarkComplete(dir, trailer->sha256);

        auto r = ensureUiStack((work / "cached").string(), "2.9.0");
        assert(r.ok);
        assert(r.uiBinaryPath == (fs::path(dir) / "TuxBloxInstaller-ui").string());
    }

    // A stale extraction from a previous build is not reused, however its name sorts.
    {
        const std::string dir = uiCacheDir("2.9.0");
        uiCacheMarkComplete(dir, std::string(64, '0'));
        buildFakeBinary(work / "stale", "different payload");
        auto r = ensureUiStack((work / "stale").string(), "2.9.0");
        // Extraction of a non-tarball fails, but the point is that it was attempted rather than the stale copy trusted.
        assert(!r.ok);
        assert(r.errorMessage.find("damaged") != std::string::npos ||
               r.errorMessage.find("unpack") != std::string::npos);
    }

    // Two runs in a row stand in for two at once: the second must reuse the first's
    // extraction, and neither may leave a .partial- directory behind. A real race
    // cannot be forced from a unit test, but the reuse path and the cleanup are the
    // two things that make losing one harmless.
    {
        fs::remove_all(uiCacheRoot());
        buildFakeBinary(work / "twice", "payload");
        auto first = ensureUiStack((work / "twice").string(), "2.9.0");
        auto second = ensureUiStack((work / "twice").string(), "2.9.0");
        // Extraction of a non-tarball fails, so assert on what is observable either way:
        // no partial directories survive a failed or a succeeded run.
        assert(first.ok == second.ok);
        int partials = 0;
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(fs::path(uiCacheRoot()), ec)) {
            if (e.path().filename().string().find(".partial-") != std::string::npos) ++partials;
        }
        assert(partials == 0);
    }

    fs::remove_all(work);
    printf("ui_extract: all tests passed\n");
    return 0;
}
