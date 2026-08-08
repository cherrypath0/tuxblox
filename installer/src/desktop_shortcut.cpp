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

#include "desktop_shortcut.h"
#include "tuxblox_logo_png.h" // generated at build time: kTuxbloxLogoPng[], kTuxbloxLogoPngLen
#include "container_env.h"
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

void runCommandBestEffort(const std::vector<std::string>& argv) {
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);
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

void createDesktopShortcut(const std::string& installDir) {
    try {
        // kTuxbloxLogoPng is already a complete, valid .png file's raw bytes
        // (FetchLogo.cmake rasterizes the logo svg to an actual PNG file,
        // then BinToHeader.cmake embeds that file's bytes verbatim) -- so
        // this is a direct byte-for-byte write, no re-encoding needed.
        const std::string iconPath = installDir + "/tuxblox.png";
        std::ofstream iconFile(iconPath, std::ios::binary);
        if (!iconFile) return;
        iconFile.write(reinterpret_cast<const char*>(kTuxbloxLogoPng),
                        static_cast<std::streamsize>(kTuxbloxLogoPngLen));
        iconFile.close();
        if (!iconFile) return;

        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";

        std::error_code ec;
        fs::create_directories(appsDir, ec);
        if (ec) return;

        const std::string desktopPath = appsDir + "/tuxblox-launcher.desktop";
        std::ofstream desktopFile(desktopPath);
        if (!desktopFile) return;
        desktopFile <<
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=TuxBlox Launcher\n"
            "Comment=Launch TuxBlox (Roblox on Linux via Proton)\n"
            "Exec=\"" << installDir << "/TuxBloxLauncher\"\n"
            "Icon=" << iconPath << "\n"
            "Terminal=false\n"
            "Categories=Game;\n";
        desktopFile.close();

        // Inside a Distrobox container, ~/.local/share/applications is
        // shared with the host (Distrobox bind-mounts $HOME by default),
        // so the .desktop file above is already visible on the host's app
        // menu -- but its Exec= path is only valid *inside* the container.
        // distrobox-export rewrites the host-visible copy's Exec= to route
        // through `distrobox-enter`. Best-effort: if the binary isn't on
        // PATH, execvp's ENOENT handling above just exits 127 harmlessly,
        // same as this file's existing behavior when e.g. an icon write
        // fails.
        if (isInsideDistrobox()) {
            runCommandBestEffort({"distrobox-export", "--app", "tuxblox-launcher"});
        }
    } catch (...) {
        // Best-effort -- a missing shortcut must not fail an otherwise
        // successful install.
    }
}

} // namespace tuxblox
