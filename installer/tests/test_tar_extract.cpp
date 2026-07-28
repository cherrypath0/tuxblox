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

// Packs a single regular-file entry under an arbitrary (possibly hostile) name.
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

// Packs a symlink entry pointing at `target`, followed by a file written
// *through* that symlink -- the classic symlink-escape archive.
static void makeSymlinkEscapeTarGz(const std::string& path, const char* linkName,
                                   const char* target, const char* fileThroughLink) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    struct archive_entry* link = archive_entry_new();
    archive_entry_set_pathname(link, linkName);
    archive_entry_set_filetype(link, AE_IFLNK);
    archive_entry_set_symlink(link, target);
    archive_entry_set_perm(link, 0777);
    archive_write_header(a, link);
    archive_entry_free(link);

    const char* content = "pwned";
    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, fileThroughLink);
    archive_entry_set_size(entry, static_cast<int64_t>(strlen(content)));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, content, strlen(content));
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
}

// Returns true if extraction threw (i.e. the entry was refused).
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

    // --- Hardening: hostile archives must be refused ---
    const fs::path tmp = fs::temp_directory_path();
    const fs::path escapeMarker = tmp / "tuxblox_test_escaped_marker.txt";
    fs::remove(escapeMarker);

    // ".."-traversal entry
    fs::path ddArchive = tmp / "tuxblox_test_dotdot.tar.gz";
    fs::path ddDest = tmp / "tuxblox_test_dotdot_dest";
    fs::remove_all(ddDest);
    makeFixtureTarGzNamed(ddArchive.string(), "../tuxblox_test_escaped_marker.txt");
    assert(extractThrows(ddArchive.string(), ddDest.string()));
    assert(!fs::exists(escapeMarker));
    fs::remove(ddArchive);
    fs::remove_all(ddDest);

    // absolute-path entry
    fs::path absArchive = tmp / "tuxblox_test_abs.tar.gz";
    fs::path absDest = tmp / "tuxblox_test_abs_dest";
    fs::remove_all(absDest);
    makeFixtureTarGzNamed(absArchive.string(), escapeMarker.string().c_str());
    assert(extractThrows(absArchive.string(), absDest.string()));
    assert(!fs::exists(escapeMarker));
    fs::remove(absArchive);
    fs::remove_all(absDest);

    // symlink-escape entry: "evil" -> <tmp>, then a file written through it
    fs::path symArchive = tmp / "tuxblox_test_symescape.tar.gz";
    fs::path symDest = tmp / "tuxblox_test_symescape_dest";
    fs::remove_all(symDest);
    makeSymlinkEscapeTarGz(symArchive.string(), "evil", tmp.string().c_str(),
                           "evil/tuxblox_test_escaped_marker.txt");
    assert(extractThrows(symArchive.string(), symDest.string()));
    assert(!fs::exists(escapeMarker));
    fs::remove(symArchive);
    fs::remove_all(symDest);

    // --- Regression: a destDir reached through a symlinked ancestor (users
    // legitimately symlink $HOME / ~/.local/share onto another disk) must
    // still extract successfully, not trip the symlink hardening. ---
    fs::path realParent = tmp / "tuxblox_test_realparent";
    fs::path linkParent = tmp / "tuxblox_test_linkparent";
    fs::remove_all(realParent);
    fs::remove(linkParent);
    fs::create_directories(realParent);
    fs::create_directory_symlink(realParent, linkParent);

    fs::path okArchive = tmp / "tuxblox_test_viasymlink.tar.gz";
    makeFixtureTarGz(okArchive.string());
    extractTarGz(okArchive.string(), (linkParent / "ProtonBuild").string());
    assert(fs::exists(realParent / "ProtonBuild" / "hello.txt"));

    fs::remove(okArchive);
    fs::remove(linkParent);
    fs::remove_all(realParent);

    printf("tar_extract: all tests passed\n");
    return 0;
}
