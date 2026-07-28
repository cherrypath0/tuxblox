#include "tar_extract.h"
#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// Rejects archive entry names that would escape destDir once prefixed:
// absolute paths ("/etc/passwd", or a Windows-style root) and any component
// equal to "..". This is the hand-rolled equivalent of
// ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS, applied to the entry's own name
// before we rewrite it to an absolute destination path.
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

void extractTarGz(const std::string& archivePath, const std::string& destDir) {
    fs::create_directories(destDir);

    // ARCHIVE_EXTRACT_SECURE_SYMLINKS (set below) rejects any path that passes
    // *through* a symlink -- and it evaluates the whole absolute path we hand
    // it, including destDir's own ancestors. Users legitimately symlink $HOME
    // or ~/.local/share onto another disk, so resolving destDir to a
    // symlink-free real path up front is what keeps that check scoped to
    // symlinks the archive itself creates (the actual attack) instead of
    // failing an ordinary install. Falls back to destDir as-given if the path
    // cannot be canonicalized.
    std::error_code canonEc;
    fs::path realDest = fs::weakly_canonical(fs::path(destDir), canonEc);
    if (canonEc || realDest.empty()) {
        realDest = fs::path(destDir);
    }

    struct archive* a = archive_read_new();
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);

    struct archive* ext = archive_write_disk_new();
    // ARCHIVE_EXTRACT_SECURE_NODOTDOT rejects ".."-traversal entries and
    // ARCHIVE_EXTRACT_SECURE_SYMLINKS rejects symlink-based escapes, both
    // evaluated against the rewritten (absolute) destination path below.
    //
    // ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS is deliberately NOT set here: it
    // is checked against the pathname we hand to archive_write_header(), and
    // we intentionally rewrite every entry to an absolute path under destDir
    // (see below), so it would reject every entry including well-formed ones.
    // The protection it provides -- refusing archive entries that carry an
    // absolute pathname of their own -- is instead enforced by
    // isUnsafeEntryPath() on the *original* entry name before rewriting.
    archive_write_disk_set_options(ext,
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
        ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_SECURE_NODOTDOT |
        ARCHIVE_EXTRACT_SECURE_SYMLINKS);

    if (archive_read_open_filename(a, archivePath.c_str(), 1 << 16) != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        archive_write_free(ext);
        throw std::runtime_error("extractTarGz: cannot open " + archivePath + ": " + err);
    }

    struct archive_entry* entry;
    for (;;) {
        int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) break;
        if (r != ARCHIVE_OK) {
            std::string err = archive_error_string(a);
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error("extractTarGz: header error: " + err);
        }

        const char* entryName = archive_entry_pathname(entry);
        if (isUnsafeEntryPath(entryName)) {
            std::string bad = entryName ? entryName : "(null)";
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error("extractTarGz: refusing unsafe archive entry path: " + bad);
        }

        fs::path entryDest = realDest / entryName;
        archive_entry_set_pathname(entry, entryDest.string().c_str());

        r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            std::string err = archive_error_string(ext);
            archive_read_free(a);
            archive_write_free(ext);
            throw std::runtime_error("extractTarGz: write header error: " + err);
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
                throw std::runtime_error("extractTarGz: read data error: " + err);
            }
            if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
                std::string err = archive_error_string(ext);
                archive_read_free(a);
                archive_write_free(ext);
                throw std::runtime_error("extractTarGz: write data error: " + err);
            }
        }
    }

    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
}

} // namespace tuxblox
