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
#include "checksum.h"
#include "tar_extract.h"
#include "ui_cache.h"
#include "ui_payload.h"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {
const char* const UiBinaryName = "TuxBloxInstaller-ui";

UiStackResult fail(const std::string& message, const std::string& detail = "") {
    UiStackResult r;
    r.errorMessage = message;
    r.errorDetail = detail;
    return r;
}

UiStackResult succeed(const std::string& dir) {
    UiStackResult r;
    r.ok = true;
    r.uiBinaryPath = (fs::path(dir) / UiBinaryName).string();
    return r;
}

// The payload is copied out to its own file because libarchive reads an archive, not a slice of a larger one.
bool copyPayload(const std::string& selfExe, const UiPayloadTrailer& trailer, const fs::path& out, std::string& detail) {
    std::ifstream in(selfExe, std::ios::binary);
    std::ofstream dst(out, std::ios::binary);
    if (!in || !dst) {
        detail = "could not open the payload for reading or the copy for writing";
        return false;
    }
    in.seekg(static_cast<std::streamoff>(trailer.offset), std::ios::beg);
    uint64_t left = trailer.size;
    char buf[65536];
    while (left > 0) {
        const std::streamsize want = static_cast<std::streamsize>(std::min<uint64_t>(left, sizeof(buf)));
        if (!in.read(buf, want)) {
            detail = "the payload ended sooner than recorded";
            return false;
        }
        dst.write(buf, want);
        if (!dst) {
            detail = "ran out of space writing the payload";
            return false;
        }
        left -= static_cast<uint64_t>(want);
    }
    dst.close();
    if (!dst) {
        detail = "ran out of space writing the payload";
        return false;
    }
    return true;
}
} // namespace

UiStackResult ensureUiStack(const std::string& selfExe, const std::string& version) {
    const auto trailer = readUiPayloadTrailer(selfExe);
    if (!trailer.has_value()) {
        return fail("This copy of the installer has no interface attached to it. Download TuxBlox again, or run this installer with --headless.");
    }

    std::string dir;
    try {
        dir = uiCacheDir(version, trailer->sha256);
    } catch (const std::exception& e) {
        return fail("TuxBlox could not work out where to unpack its interface. Run this installer with --headless instead.", e.what());
    }

    if (uiCacheIsComplete(dir, trailer->sha256)) {
        return succeed(dir);
    }

    const fs::path partial = dir + ".partial-" + std::to_string(getpid());
    std::error_code ec;
    fs::remove_all(partial, ec);
    fs::create_directories(partial, ec);
    if (ec) {
        return fail("TuxBlox could not create " + fs::path(dir).parent_path().string() + " to unpack its interface. Check that you are allowed to write there and that the disk is not full, or run this installer with --headless.", ec.message());
    }

    const fs::path archive = partial / "payload.tar.zst";
    std::string detail;
    if (!copyPayload(selfExe, *trailer, archive, detail)) {
        fs::remove_all(partial, ec);
        return fail("TuxBlox could not read its interface out of this installer. Check that the disk is not full, or run this installer with --headless.", detail);
    }

    std::string actual;
    try {
        actual = sha256File(archive.string());
    } catch (const std::exception& e) {
        fs::remove_all(partial, ec);
        return fail("TuxBlox could not check its interface. Run this installer with --headless instead.", e.what());
    }
    if (actual != trailer->sha256) {
        fs::remove_all(partial, ec);
        return fail("This copy of the installer is damaged: its interface does not match its own checksum. Download TuxBlox again, or run this installer with --headless.");
    }

    try {
        extractTarZst(archive.string(), partial.string());
    } catch (const std::exception& e) {
        fs::remove_all(partial, ec);
        return fail("TuxBlox could not unpack its interface. Check that the disk is not full, or run this installer with --headless.", e.what());
    }
    fs::remove(archive, ec);

    // The mode bits rather than access(), which also fails on a folder mounted to forbid running programs and would blame the download for it.
    const fs::path binary = partial / UiBinaryName;
    const fs::perms mode = fs::status(binary, ec).permissions();
    const bool runnable = fs::is_regular_file(binary, ec) && (mode & (fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec)) != fs::perms::none;
    if (!runnable) {
        fs::remove_all(partial, ec);
        return fail("This copy of the installer is damaged: its interface is missing the program that should start it. Download TuxBlox again, or run this installer with --headless.");
    }
    uiCacheMarkComplete(partial.string(), trailer->sha256);

    // The folder is named after this exact payload, so rename() failing means someone else published the identical tree, and their copy is just as good.
    fs::rename(partial, dir, ec);
    if (ec) {
        const std::string renameError = ec.message();
        const bool winnerIsComplete = uiCacheIsComplete(dir, trailer->sha256);
        fs::remove_all(partial, ec);
        if (winnerIsComplete) {
            return succeed(dir);
        }
        return fail("TuxBlox could not finish unpacking its interface. Run this installer with --headless instead.", renameError);
    }

    // Always the cache folder itself: the prune removes every ui-* folder beside whatever it is given.
    uiCachePruneOthers(dir);
    return succeed(dir);
}

} // namespace tuxblox
