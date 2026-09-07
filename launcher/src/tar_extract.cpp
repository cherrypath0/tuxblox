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
#include <functional>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// What to do with one archive entry, decided from its stored path.
enum class EntryPath {
    Ok,     // safe to extract, under the relative path handed back
    Skip,   // names no file of its own -- nothing to extract
    Unsafe, // tries to escape the destination
};

// Rewrites an archive entry path into one that is safe to join onto the
// destination, the way tar and unzip do: any root prefix is dropped so the
// entry always lands inside destDir. Roblox's packages need this -- they mark
// every directory with a leading separator ("\\Qt5\\", which libarchive reads
// back as "/Qt5/") and open with a bare root marker that names no file at all.
// A ".." component is a real escape attempt and is still refused.
EntryPath vetEntryPath(const char* name, fs::path& relative) {
    if (name == nullptr || name[0] == '\0') return EntryPath::Skip;

    fs::path cleaned;
    for (const auto& part : fs::path(name).relative_path()) {
        if (part == "..") return EntryPath::Unsafe;
        if (part == "." || part.empty()) continue;
        cleaned /= part;
    }
    if (cleaned.empty()) return EntryPath::Skip;

    relative = cleaned;
    return EntryPath::Ok;
}

using ConfigureReaderFn = std::function<void(struct archive*)>;

void extractArchive(const std::string& archivePath, const std::string& destDir,
                     const TarExtractProgressFn& onProgress, const ConfigureReaderFn& configureReader,
                     const char* fnName) {
    fs::create_directories(destDir);

    std::error_code canonEc;
    fs::path realDest = fs::weakly_canonical(fs::path(destDir), canonEc);
    if (canonEc || realDest.empty()) {
        realDest = fs::path(destDir);
    }

    uint64_t totalBytes = 0;
    {
        std::error_code sizeEc;
        auto sz = fs::file_size(archivePath, sizeEc);
        if (!sizeEc) totalBytes = static_cast<uint64_t>(sz);
    }

    struct archive* a = archive_read_new();
    configureReader(a);

    struct archive* ext = archive_write_disk_new();
    archive_write_disk_set_options(ext,
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_SECURE_NODOTDOT |
        ARCHIVE_EXTRACT_SECURE_SYMLINKS);

    if (archive_read_open_filename(a, archivePath.c_str(), 1 << 16) != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        archive_write_free(ext);
        throw std::runtime_error(std::string(fnName) + ": cannot open " + archivePath + ": " + err);
    }

    struct archive_entry* entry;
    for (;;) {
        int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) break;
        if (r != ARCHIVE_OK) {
            std::string err = archive_error_string(a);
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error(std::string(fnName) + ": header error: " + err);
        }

        const char* entryName = archive_entry_pathname(entry);
        fs::path entryRelative;
        EntryPath verdict = vetEntryPath(entryName, entryRelative);
        if (verdict == EntryPath::Skip) continue;
        if (verdict == EntryPath::Unsafe) {
            std::string bad = entryName ? entryName : "(null)";
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error(std::string(fnName) + ": refusing unsafe archive entry path: " + bad);
        }

        fs::path entryDest = realDest / entryRelative;
        archive_entry_set_pathname(entry, entryDest.string().c_str());

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            std::string err = archive_error_string(ext);
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error(std::string(fnName) + ": write header error: " + err);
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
                throw std::runtime_error(std::string(fnName) + ": read data error: " + err);
            }
            if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
                std::string err = archive_error_string(ext);
                archive_read_free(a);
                archive_write_free(ext);
                throw std::runtime_error(std::string(fnName) + ": write data error: " + err);
            }
        }

        if (onProgress) {
            int64_t consumed = archive_filter_bytes(a, -1);
            if (consumed >= 0) {
                onProgress(static_cast<uint64_t>(consumed), totalBytes);
            }
        }
    }

    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
}

} // namespace

void extractTarZst(const std::string& archivePath, const std::string& destDir,
                    const TarExtractProgressFn& onProgress) {
    extractArchive(archivePath, destDir, onProgress,
        [](struct archive* a) {
            archive_read_support_filter_zstd(a);
            archive_read_support_format_tar(a);
        },
        "extractTarZst");
}

void extractZip(const std::string& archivePath, const std::string& destDir,
                 const TarExtractProgressFn& onProgress) {
    extractArchive(archivePath, destDir, onProgress,
        [](struct archive* a) { archive_read_support_format_zip(a); },
        "extractZip");
}

} // namespace tuxblox
