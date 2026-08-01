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
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static void makeFixtureTarGz(const std::string& path) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_gzip(a);
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

static void makeFixtureTarGzNamed(const std::string& path, const char* entryName) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_gzip(a);
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
        tuxblox::extractTarGz(archivePath, destDir);
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

int main() {
    using tuxblox::extractTarGz;

    fs::path archivePath = fs::temp_directory_path() / "tuxblox_test_archive.tar.gz";
    fs::path destDir = fs::temp_directory_path() / "tuxblox_test_archive_extracted";

    fs::remove_all(destDir);
    makeFixtureTarGz(archivePath.string());

    extractTarGz(archivePath.string(), destDir.string());

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

    printf("tar_extract: all tests passed\n");
    return 0;
}
