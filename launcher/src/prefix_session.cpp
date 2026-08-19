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

#include "prefix_session.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// Kept in sync with SESSION_HOLDER_IMAGES in ProtonSource/proton.py (line 517).
// Deliberately an allowlist of the apps this launcher exists to run, not a
// denylist of helpers -- same reasoning as proton.py's own comment: forgetting
// a helper only costs a wrong verb choice, forgetting an app breaks a launch.
const char* const kSessionHolderImages[] = {
    "robloxplayerbeta.exe",
    "robloxstudiobeta.exe",
    "robloxplayerinstaller.exe",
    "robloxstudioinstaller.exe",
};

std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string readWholeFile(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// os.path.normpath's shape for our purposes: collapse the path and drop any
// trailing separator, so ".../pfx/" and ".../pfx" compare equal. proton.py sets
// WINEPREFIX with a trailing slash (prefix_dir = path("pfx/"), line 774) and
// pid_wineprefix() normpaths it away before comparing -- do the same on both
// sides or the comparison never matches.
std::string normalizePath(const std::string& p) {
    if (p.empty()) return p;
    std::string s = fs::path(p).lexically_normal().string();
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

std::string wineprefixFromEnviron(const std::string& environBlob) {
    static const std::string key = "WINEPREFIX=";
    size_t pos = 0;
    while (pos < environBlob.size()) {
        size_t end = environBlob.find('\0', pos);
        if (end == std::string::npos) end = environBlob.size();
        if (environBlob.compare(pos, key.size(), key) == 0)
            return normalizePath(environBlob.substr(pos + key.size(), end - pos - key.size()));
        pos = end + 1;
    }
    return "";
}

bool isSessionHolderImage(const std::string& image) {
    if (image.empty()) return false;
    for (const char* candidate : kSessionHolderImages)
        if (image == candidate) return true;
    return false;
}

} // namespace

std::string wineImageNameFromCmdline(const std::string& firstCmdlineToken) {
    // A wine process's /proc/<pid>/cmdline holds the *Windows* command line
    // ("C:\...\RobloxStudioBeta.exe -foo"). comm is no use here: it holds
    // whatever the app named its main thread, which for Studio is literally
    // "Main". Cut at the first ".exe", not the first space -- the image path
    // itself can contain spaces, and the arguments after it can contain further
    // ".exe" paths (RobloxCrashHandler's --attachment= list, for one). Same rule
    // as pid_wine_image() in ProtonSource/proton.py.
    const std::string lower = toLower(firstCmdlineToken);
    const size_t cut = lower.find(".exe");
    if (cut == std::string::npos) return "";
    std::string image = lower.substr(0, cut + 4);
    for (char& c : image) if (c == '/') c = '\\';
    const size_t slash = image.rfind('\\');
    return slash == std::string::npos ? image : image.substr(slash + 1);
}

bool prefixHasSessionHolderIn(const std::string& procRoot, const std::string& prefixDir) {
    const std::string want = normalizePath(prefixDir);
    if (want.empty()) return false;

    std::error_code ec;
    fs::directory_iterator it(procRoot, ec);
    if (ec) return false; // no /proc, or not readable -- "nothing running", not an error

    for (const auto& entry : it) {
        const std::string name = entry.path().filename().string();
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;

        // Image name first: cmdline is world-readable and cheap, and it narrows
        // a few hundred processes down to the handful worth reading environ for
        // (environ is not world-readable). Same ordering as proton.py.
        const std::string cmdline = readWholeFile(entry.path() / "cmdline");
        const size_t nul = cmdline.find('\0');
        const std::string first = nul == std::string::npos ? cmdline : cmdline.substr(0, nul);
        if (!isSessionHolderImage(wineImageNameFromCmdline(first))) continue;

        if (wineprefixFromEnviron(readWholeFile(entry.path() / "environ")) == want) return true;
    }
    return false;
}

bool prefixHasSessionHolder(const std::string& prefixDir) {
    return prefixHasSessionHolderIn("/proc", prefixDir);
}

} // namespace tuxblox
