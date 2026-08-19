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

#include "uninstall.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// Same shape as desktop_shortcut.cpp's helper of the same name -- kept as
// its own copy rather than a shared header since each is a tiny,
// self-contained fork/exec/bounded-wait, not worth a cross-file dependency.
void runCommandBestEffort(const std::vector<std::string>& argv) {
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || r < 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace

void removeDesktopFiles(const std::string& appsDir) {
    std::error_code ec;
    if (!fs::exists(appsDir, ec)) return;
    for (const auto& entry : fs::directory_iterator(appsDir, ec)) {
        if (ec) return;
        const std::string name = entry.path().filename().string();
        if (name.rfind("tuxblox-", 0) == 0 && entry.path().extension() == ".desktop") {
            std::error_code removeEc;
            fs::remove(entry.path(), removeEc); // best-effort per file
        }
    }
}

void removeTuxBloxIcons(const std::string& hicolorDir) {
    // Same buckets desktop_integration.cpp's writeDesktopEntries() and
    // wine_shortcut_export.cpp's copyIcons() populate.
    static const char* const kIconSizes[] = {"16x16", "24x24", "32x32", "48x48",
                                              "64x64", "96x96", "128x128", "256x256"};
    static const char* const kIconNames[] = {"tuxblox", "tuxblox-roblox-studio",
                                              "tuxblox-roblox-player"};
    for (const char* size : kIconSizes) {
        for (const char* name : kIconNames) {
            std::error_code ec;
            fs::remove(fs::path(hicolorDir) / size / "apps" / (std::string(name) + ".png"), ec);
        }
    }
}

void removeMimePackage(const std::string& xmlPath) {
    std::error_code ec;
    fs::remove(xmlPath, ec); // best-effort -- fine if it never existed
}

void stripMimeappsAssociations(const std::string& mimeappsPath) {
    std::ifstream in(mimeappsPath);
    if (!in) return; // doesn't exist -- nothing to strip

    std::vector<std::string> kept;
    std::string line;
    bool changed = false;
    while (std::getline(in, line)) {
        // Substring match on the "tuxblox-" id prefix, not just the retired
        // single-entry url handler: ensureDesktopIntegration() now also
        // writes per-scheme handler associations (tuxblox-roblox-handler.desktop,
        // tuxblox-player-handler.desktop, tuxblox-studio-handler.desktop) and
        // the place-file handler (tuxblox-studio-place.desktop) into this same
        // file, and uninstall must not leave any of them behind.
        if (line.find("tuxblox-") != std::string::npos) {
            changed = true;
            continue;
        }
        kept.push_back(line);
    }
    in.close();
    if (!changed) return;

    std::ofstream out(mimeappsPath, std::ios::trunc);
    if (!out) return;
    for (const auto& l : kept) out << l << "\n";
}

bool removeInstallDir(const std::string& installDir) {
    std::error_code ec;
    if (!fs::exists(installDir, ec)) return true; // already gone -- success
    fs::remove_all(installDir, ec);
    return !ec;
}

bool performUninstall() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') return false;

    const std::string appsDir = std::string(home) + "/.local/share/applications";
    removeDesktopFiles(appsDir);
    stripMimeappsAssociations(std::string(home) + "/.config/mimeapps.list");
    stripMimeappsAssociations(appsDir + "/mimeapps.list"); // legacy xdg location
    // Was a single 256x256 removal, which left the other seven size buckets
    // writeDesktopEntries() populates behind on every uninstall.
    removeTuxBloxIcons(std::string(home) + "/.local/share/icons/hicolor");
    removeMimePackage(std::string(home) + "/.local/share/mime/packages/tuxblox-roblox-place.xml");

    if (!std::getenv("TUXBLOX_SKIP_XDG_MIME")) { // escape hatch for sandboxed test/CI runs
        runCommandBestEffort({"update-desktop-database", appsDir});
        runCommandBestEffort({"gtk-update-icon-cache", std::string(home) + "/.local/share/icons/hicolor"});
        runCommandBestEffort({"update-mime-database", std::string(home) + "/.local/share/mime"});
    }

    return removeInstallDir(std::string(home) + "/.tuxblox");
}

} // namespace tuxblox
