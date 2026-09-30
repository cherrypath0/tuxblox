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
#include <archive.h>
#include <archive_entry.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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

static void addFile(struct archive* a, const char* name, const std::string& content, int perm) {
    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, name);
    archive_entry_set_size(entry, static_cast<int64_t>(content.size()));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, perm);
    archive_write_header(a, entry);
    archive_write_data(a, content.data(), content.size());
    archive_entry_free(entry);
}

// A genuine zstd tarball shaped like the real payload: the interface binary at the root and a library beside it.
static std::string buildRealPayload(const fs::path& path) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());
    addFile(a, "TuxBloxInstaller-ui", "#!/bin/sh\n", 0755);
    addFile(a, "lib/libdummy.so", "dummy", 0644);
    archive_write_close(a);
    archive_write_free(a);
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static int countPartials() {
    int partials = 0;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(fs::path(tuxblox::uiCacheRoot()), ec)) {
        if (e.path().filename().string().find(".partial-") != std::string::npos) ++partials;
    }
    return partials;
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

    // A real race cannot be forced from a unit test, so two runs in a row stand in for it and neither may leave a partial folder behind.
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

    // A real archive is verified, unpacked, published, recorded as complete, and then reused rather than unpacked again.
    {
        fs::remove_all(uiCacheRoot());
        const std::string payload = buildRealPayload(work / "real.tar.zst");
        buildFakeBinary(work / "real", payload);
        auto trailer = readUiPayloadTrailer((work / "real").string());
        assert(trailer.has_value());

        auto first = ensureUiStack((work / "real").string(), "2.9.0");
        assert(first.ok);
        assert(fs::exists(first.uiBinaryPath));
        assert(first.uiBinaryPath == (fs::path(uiCacheDir("2.9.0")) / "TuxBloxInstaller-ui").string());
        assert(fs::exists(fs::path(uiCacheDir("2.9.0")) / "lib" / "libdummy.so"));
        assert(uiCacheIsComplete(uiCacheDir("2.9.0"), trailer->sha256));
        {
            std::ifstream marker(fs::path(uiCacheDir("2.9.0")) / ".complete");
            std::string recorded;
            marker >> recorded;
            assert(recorded == trailer->sha256);
        }
        assert(countPartials() == 0);

        { std::ofstream sentinel(fs::path(uiCacheDir("2.9.0")) / "sentinel"); sentinel << "x"; }
        auto second = ensureUiStack((work / "real").string(), "2.9.0");
        assert(second.ok);
        assert(second.uiBinaryPath == first.uiBinaryPath);
        assert(fs::exists(fs::path(uiCacheDir("2.9.0")) / "sentinel"));
        assert(countPartials() == 0);
    }

    // A payload that unpacks fine but lacks the interface program is damaged, not a permissions problem.
    {
        fs::remove_all(uiCacheRoot());
        struct archive* a = archive_write_new();
        archive_write_add_filter_zstd(a);
        archive_write_set_format_pax_restricted(a);
        archive_write_open_filename(a, (work / "wrong.tar.zst").c_str());
        addFile(a, "lib/libdummy.so", "dummy", 0644);
        archive_write_close(a);
        archive_write_free(a);
        std::ifstream in(work / "wrong.tar.zst", std::ios::binary);
        buildFakeBinary(work / "wrong", std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()));
        auto r = ensureUiStack((work / "wrong").string(), "2.9.0");
        assert(!r.ok);
        assert(r.errorMessage.find("damaged") != std::string::npos);
        assert(countPartials() == 0);
    }

    fs::remove_all(work);
    printf("ui_extract: all tests passed\n");
    return 0;
}
