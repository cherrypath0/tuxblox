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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {

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
    } catch (...) {
        // Best-effort -- a missing shortcut must not fail an otherwise
        // successful install.
    }
}

} // namespace tuxblox
