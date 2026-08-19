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

#include "prefix_file_bridge.h"
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {

const char* const kBridgeWindowsRoot = "C:\\users\\user\\Documents\\TuxBlox Files";

namespace {

std::string driveCRoot(const std::string& installDir) {
    return installDir + "/runtime/pfx/drive_c";
}

std::string bridgeRootDir(const std::string& installDir) {
    return driveCRoot(installDir) + "/users/user/Documents/TuxBlox Files";
}

// A host path under drive_c is already reachable -- turn it into an ordinary
// C:\ path directly rather than symlinking the prefix into itself.
std::string windowsPathUnderDriveC(const fs::path& driveC, const fs::path& hostPath) {
    const fs::path rel = hostPath.lexically_relative(driveC);
    if (rel.empty()) return "";
    const std::string relStr = rel.generic_string();
    if (relStr == "." || relStr.rfind("..", 0) == 0) return "";
    std::string win = "C:\\" + relStr;
    for (char& c : win) if (c == '/') c = '\\';
    return win;
}

// Removes symlinks in the bridge root whose target no longer exists. Live links
// are left alone on purpose -- Studio's recent-files list keeps resolving
// through them, and dropping one would break "reopen last place".
void pruneDanglingLinks(const fs::path& root) {
    std::error_code ec;
    fs::directory_iterator it(root, ec);
    if (ec) return;
    for (const auto& entry : it) {
        std::error_code linkEc;
        if (!fs::is_symlink(entry.symlink_status(linkEc)) || linkEc) continue;
        std::error_code existsEc;
        if (!fs::exists(entry.path(), existsEc) || existsEc) {
            std::error_code rmEc;
            fs::remove(entry.path(), rmEc);
        }
    }
}

} // namespace

std::string sanitizeBridgeLinkName(const std::string& name) {
    static const std::string illegal = "<>:\"/\\|?*";
    std::string out;
    for (char c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);
        out.push_back(uc < 0x20 || illegal.find(c) != std::string::npos ? '_' : c);
    }
    // Windows rejects a trailing space or dot in a directory name.
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    return out.empty() ? "root" : out;
}

std::string bridgeHostPathIntoPrefix(const std::string& installDir, const std::string& hostPath) {
    try {
        std::error_code ec;
        const fs::path canonical = fs::canonical(hostPath, ec);
        if (ec) return ""; // doesn't exist / unreadable -- caller reports this

        const fs::path canonicalDriveC = fs::canonical(driveCRoot(installDir), ec);
        if (!ec) {
            const std::string direct = windowsPathUnderDriveC(canonicalDriveC, canonical);
            if (!direct.empty()) return direct;
        }

        const fs::path parent = canonical.parent_path();
        const fs::path root = bridgeRootDir(installDir);
        fs::create_directories(root, ec);
        if (ec) return "";
        pruneDanglingLinks(root);

        const std::string base = sanitizeBridgeLinkName(parent.filename().string());
        for (int attempt = 1; attempt <= 32; ++attempt) {
            const std::string linkName =
                attempt == 1 ? base : base + "-" + std::to_string(attempt);
            const fs::path link = root / linkName;

            std::error_code statEc;
            if (fs::exists(fs::symlink_status(link, statEc)) && !statEc) {
                // Reuse a link that already points where we want; step past one
                // that belongs to a different directory with the same basename.
                std::error_code readEc;
                const fs::path target = fs::read_symlink(link, readEc);
                if (readEc) continue;
                const fs::path resolved = fs::weakly_canonical(target, readEc);
                if (readEc || resolved != parent) continue;
            } else {
                std::error_code linkEc;
                fs::create_directory_symlink(parent, link, linkEc);
                if (linkEc) continue;
            }
            return std::string(kBridgeWindowsRoot) + "\\" + linkName + "\\" +
                   canonical.filename().string();
        }
        return ""; // 32 same-named directories already bridged -- give up rather than guess
    } catch (...) {
        return "";
    }
}

} // namespace tuxblox
