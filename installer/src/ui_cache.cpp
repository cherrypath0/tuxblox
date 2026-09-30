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

#include "ui_cache.h"
#include "install_paths.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {
// An environment variable naming a folder is only usable when it is absolute, the same rule TUXBLOX_ROOT follows.
const char* absoluteEnv(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') return nullptr;
    if (value[0] != '/') throw std::runtime_error(std::string(name) + " must be an absolute path");
    return value;
}
} // namespace

std::string uiCacheRoot() {
    if (const char* xdg = absoluteEnv("XDG_CACHE_HOME")) {
        return (fs::path(xdg) / "tuxblox").string();
    }
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        throw std::runtime_error("Neither XDG_CACHE_HOME nor HOME is set, so there is nowhere to unpack the interface");
    }
    return (fs::path(home) / ".cache" / "tuxblox").string();
}

std::string uiCacheDir(const std::string& version) {
    return (fs::path(uiCacheRoot()) / ("ui-" + version)).string();
}

bool uiCacheIsComplete(const std::string& dir, const std::string& sha256) {
    std::ifstream in(fs::path(dir) / ".complete");
    if (!in) return false;
    std::string recorded;
    if (!(in >> recorded)) return false;
    return recorded == sha256;
}

void uiCacheMarkComplete(const std::string& dir, const std::string& sha256) {
    std::ofstream out(fs::path(dir) / ".complete", std::ios::trunc);
    out << sha256 << "\n";
}

void uiCachePruneOthers(const std::string& keepDir) {
    try {
        const fs::path root = fs::path(normalizedDir(keepDir)).parent_path();
        const std::string keep = normalizedDir(keepDir);
        std::error_code ec;
        auto it = fs::directory_iterator(root, ec);
        const auto end = fs::directory_iterator();
        while (it != end) {
            std::error_code entry_ec;
            const auto entry = *it;
            ++it;
            if (!entry.is_directory(entry_ec) || entry_ec) continue;
            const std::string name = entry.path().filename().string();
            if (name.rfind("ui-", 0) != 0) continue;
            if (normalizedDir(entry.path().string()) == keep) continue;
            std::error_code removeEc;
            fs::remove_all(entry.path(), removeEc);
        }
    } catch (...) {
    }
}

} // namespace tuxblox
