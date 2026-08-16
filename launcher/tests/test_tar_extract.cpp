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

#include "tar_extract.h"
#include <archive.h>
#include <archive_entry.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

static void makeFixtureTarGz(const std::string& path) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    const char* content = "hello from tuxblox test fixture";
    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, "hello.txt");
    archive_entry_set_size(entry, static_cast<int64_t>(strlen(content)));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, content, strlen(content));
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
}

// Packs two small regular-file entries, so a per-entry progress callback
// fires more than once -- used only by the extraction-progress test below,
// so its monotonicity assertion (nowSamples[i] >= nowSamples[i - 1]) has
// more than one sample to actually compare.
static void makeFixtureTarGzTwoEntries(const std::string& path) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    const char* contents[2] = {"hello from entry one", "hello from entry two, slightly longer"};
    const char* names[2] = {"hello1.txt", "hello2.txt"};
    for (int i = 0; i < 2; ++i) {
        struct archive_entry* entry = archive_entry_new();
        archive_entry_set_pathname(entry, names[i]);
        archive_entry_set_size(entry, static_cast<int64_t>(strlen(contents[i])));
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, contents[i], strlen(contents[i]));
        archive_entry_free(entry);
    }

    archive_write_close(a);
    archive_write_free(a);
}

static void makeFixtureTarGzNamed(const std::string& path, const char* entryName) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    const char* content = "pwned";
    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, entryName);
    archive_entry_set_size(entry, static_cast<int64_t>(strlen(content)));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, content, strlen(content));
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
}

static bool extractThrows(const std::string& archivePath, const std::string& destDir) {
    try {
        tuxblox::extractTarZst(archivePath, destDir);
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

static void makeFixtureZip(const std::string& path, const char* entryName, const char* content) {
    struct archive* a = archive_write_new();
    archive_write_set_format_zip(a);
    archive_write_open_filename(a, path.c_str());

    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, entryName);
    archive_entry_set_size(entry, static_cast<int64_t>(strlen(content)));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, content, strlen(content));
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
}

static bool extractZipThrows(const std::string& archivePath, const std::string& destDir) {
    try {
        tuxblox::extractZip(archivePath, destDir);
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

int main() {
    using tuxblox::extractTarZst;

    fs::path archivePath = fs::temp_directory_path() / "tuxblox_test_archive.tar.gz";
    fs::path destDir = fs::temp_directory_path() / "tuxblox_test_archive_extracted";

    fs::remove_all(destDir);
    makeFixtureTarGz(archivePath.string());

    extractTarZst(archivePath.string(), destDir.string());

    std::ifstream in(destDir / "hello.txt");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(content == "hello from tuxblox test fixture");

    fs::remove(archivePath);
    fs::remove_all(destDir);

    // Hostile archives must be refused.
    const fs::path tmp = fs::temp_directory_path();
    const fs::path escapeMarker = tmp / "tuxblox_test_escaped_marker.txt";
    fs::remove(escapeMarker);

    fs::path ddArchive = tmp / "tuxblox_test_dotdot.tar.gz";
    fs::path ddDest = tmp / "tuxblox_test_dotdot_dest";
    fs::remove_all(ddDest);
    makeFixtureTarGzNamed(ddArchive.string(), "../tuxblox_test_escaped_marker.txt");
    assert(extractThrows(ddArchive.string(), ddDest.string()));
    assert(!fs::exists(escapeMarker));
    fs::remove(ddArchive);
    fs::remove_all(ddDest);

    fs::path absArchive = tmp / "tuxblox_test_abs.tar.gz";
    fs::path absDest = tmp / "tuxblox_test_abs_dest";
    fs::remove_all(absDest);
    makeFixtureTarGzNamed(absArchive.string(), escapeMarker.string().c_str());
    assert(extractThrows(absArchive.string(), absDest.string()));
    assert(!fs::exists(escapeMarker));
    fs::remove(absArchive);
    fs::remove_all(absDest);

    // Extraction progress: onProgress must be called at least once and
    // report a monotonically non-decreasing, always-bounded-by-total byte
    // count -- this is what lets the installer's ExtractingProton step
    // show incremental progress instead of jumping straight from 0% to
    // 100% (see installer_steps.cpp's Step::ExtractingProton report()).
    {
        fs::path progArchive = tmp / "tuxblox_test_progress.tar.gz";
        fs::path progDest = tmp / "tuxblox_test_progress_dest";
        fs::remove_all(progDest);
        // Two entries so onProgress fires more than once (it fires once per
        // archive entry) -- otherwise the monotonicity check below never
        // actually compares two samples.
        makeFixtureTarGzTwoEntries(progArchive.string());

        std::vector<uint64_t> nowSamples;
        std::vector<uint64_t> totalSamples;
        extractTarZst(progArchive.string(), progDest.string(),
            [&](uint64_t now, uint64_t total) {
                nowSamples.push_back(now);
                totalSamples.push_back(total);
            });

        assert(nowSamples.size() >= 2); // two entries -> at least two progress callbacks
        uint64_t archiveSize = fs::file_size(progArchive);
        for (size_t i = 0; i < nowSamples.size(); ++i) {
            assert(totalSamples[i] == archiveSize);
            assert(nowSamples[i] <= totalSamples[i]);
            if (i > 0) assert(nowSamples[i] >= nowSamples[i - 1]);
        }

        fs::remove(progArchive);
        fs::remove_all(progDest);
    }

    // extractZip(): basic round-trip.
    {
        fs::path zipPath = tmp / "tuxblox_test_archive.zip";
        fs::path zipDest = tmp / "tuxblox_test_zip_extracted";
        fs::remove_all(zipDest);
        makeFixtureZip(zipPath.string(), "RobloxApp.exe", "fake roblox binary contents");

        tuxblox::extractZip(zipPath.string(), zipDest.string());

        std::ifstream in(zipDest / "RobloxApp.exe");
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content == "fake roblox binary contents");

        fs::remove(zipPath);
        fs::remove_all(zipDest);
    }

    // extractZip(): same path-traversal protection as extractTarZst.
    {
        fs::path zipPath = tmp / "tuxblox_test_dotdot.zip";
        fs::path zipDest = tmp / "tuxblox_test_zip_dotdot_dest";
        fs::remove_all(zipDest);
        makeFixtureZip(zipPath.string(), "../tuxblox_test_escaped_marker.txt", "pwned");
        assert(extractZipThrows(zipPath.string(), zipDest.string()));
        assert(!fs::exists(escapeMarker));
        fs::remove(zipPath);
        fs::remove_all(zipDest);
    }

    printf("tar_extract: all tests passed\n");
    return 0;
}
