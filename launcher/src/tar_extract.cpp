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
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

bool isUnsafeEntryPath(const char* name) {
    if (name == nullptr || name[0] == '\0') return true;
    fs::path p(name);
    if (p.is_absolute() || p.has_root_path() || name[0] == '/') return true;
    for (const auto& part : p) {
        if (part == "..") return true;
    }
    return false;
}

} // namespace

void extractTarZst(const std::string& archivePath, const std::string& destDir) {
    fs::create_directories(destDir);

    std::error_code canonEc;
    fs::path realDest = fs::weakly_canonical(fs::path(destDir), canonEc);
    if (canonEc || realDest.empty()) {
        realDest = fs::path(destDir);
    }

    struct archive* a = archive_read_new();
    archive_read_support_filter_zstd(a);
    archive_read_support_format_tar(a);

    struct archive* ext = archive_write_disk_new();
    archive_write_disk_set_options(ext,
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_SECURE_NODOTDOT |
        ARCHIVE_EXTRACT_SECURE_SYMLINKS);

    if (archive_read_open_filename(a, archivePath.c_str(), 1 << 16) != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        archive_write_free(ext);
        throw std::runtime_error("extractTarZst: cannot open " + archivePath + ": " + err);
    }

    struct archive_entry* entry;
    for (;;) {
        int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) break;
        if (r != ARCHIVE_OK) {
            std::string err = archive_error_string(a);
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error("extractTarZst: header error: " + err);
        }

        const char* entryName = archive_entry_pathname(entry);
        if (isUnsafeEntryPath(entryName)) {
            std::string bad = entryName ? entryName : "(null)";
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error("extractTarZst: refusing unsafe archive entry path: " + bad);
        }

        fs::path entryDest = realDest / entryName;
        archive_entry_set_pathname(entry, entryDest.string().c_str());

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            std::string err = archive_error_string(ext);
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error("extractTarZst: write header error: " + err);
        }

        const void* buff;
        size_t size;
        int64_t offset;
        for (;;) {
            r = archive_read_data_block(a, &buff, &size, &offset);
            if (r == ARCHIVE_EOF) break;
            if (r != ARCHIVE_OK) {
                std::string err = archive_error_string(a);
                archive_read_free(a);
                archive_write_free(ext);
                throw std::runtime_error("extractTarZst: read data error: " + err);
            }
            if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
                std::string err = archive_error_string(ext);
                archive_read_free(a);
                archive_write_free(ext);
                throw std::runtime_error("extractTarZst: write data error: " + err);
            }
        }
    }

    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
}

} // namespace tuxblox
