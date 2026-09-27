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

#include "install_paths.h"
#include <cstdio>
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <system_error>
#include <sys/statvfs.h>
#include <unistd.h>

namespace tuxblox {

std::string normalizedDir(const std::string& dir) {
    std::string path = std::filesystem::path(dir).lexically_normal().string();
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

std::string existingAncestor(const std::string& dir) {
    namespace fs = std::filesystem;
    std::error_code error;
    fs::path path = normalizedDir(dir);
    while (!path.empty() && path != path.root_path() && !fs::exists(path, error)) {
        path = path.parent_path();
    }
    return path.empty() ? std::string("/") : path.string();
}

bool looksLikeInstall(const std::string& dir) {
    static const char* const markers[] = {
        "/TuxBloxLauncher",
        "/compat/main",
        "/proton/main",
        "/COPYRIGHT.txt",
    };
    for (const char* marker : markers) {
        if (access((dir + marker).c_str(), F_OK) == 0) return true;
    }
    return false;
}

std::string selfExePath() {
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf);
}

std::string installDir() {
    const char* root = std::getenv("TUXBLOX_ROOT");
    if (root && root[0] != '\0') {
        if (root[0] == '/') return normalizedDir(root);
        fprintf(stderr, "TuxBlox: ignoring TUXBLOX_ROOT, it is not an absolute path: %s\n", root);
    }

    // The persisted installer lives in the install folder and is re-run from there to apply an update, so its own directory is the right answer -- but only then, since a freshly downloaded one sits in Downloads.
    const std::string self = selfExePath();
    const std::size_t slash = self.rfind('/');
    if (slash != std::string::npos && slash > 0) {
        const std::string dir = self.substr(0, slash);
        if (looksLikeInstall(dir)) return dir;
    }

    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
        throw std::runtime_error("installDir: HOME environment variable is not set");
    }
    return std::string(home) + "/.tuxblox";
}

bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes) {
    struct statvfs st{};
    if (statvfs(path.c_str(), &st) != 0) {
        throw std::runtime_error("hasEnoughDiskSpace: statvfs failed for " + path);
    }
    uint64_t freeBytes = static_cast<uint64_t>(st.f_bavail) * static_cast<uint64_t>(st.f_frsize);
    return freeBytes >= minBytes;
}

} // namespace tuxblox
