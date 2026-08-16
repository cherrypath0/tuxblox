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
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int main() {
    using namespace tuxblox;

    fs::path work = fs::temp_directory_path() / "tuxblox_test_desktop_shortcut";
    fs::remove_all(work);
    fs::create_directories(work);

    // Both functions write into $HOME/.local/share/{applications,icons} --
    // point HOME at a scratch directory so this never touches the real one.
    // TUXBLOX_SKIP_XDG_MIME suppresses refreshUrlHandlers' xdg-mime/
    // update-desktop-database calls, same escape hatch the code documents for
    // sandboxed CI runs. (createDesktopShortcut's own gtk-update-icon-cache
    // call has no such guard, but it's a best-effort fork/exec that just
    // exits 127 when the binary isn't on PATH.)
    fs::path fakeHome = work / "home";
    fs::create_directories(fakeHome);
    setenv("HOME", fakeHome.c_str(), 1);
    setenv("TUXBLOX_SKIP_XDG_MIME", "1", 1);

    const fs::path appsDir = fakeHome / ".local" / "share" / "applications";
    const fs::path menuEntry = appsDir / "tuxblox-launcher.desktop";

    // The case this file exists for: an archive-shaped launcher artifact
    // installs the executable *inside* its own bundle directory, so the
    // Exec= line must name that full path. The previous implementation
    // derived it as installDir + "/TuxBloxLauncher", which for this layout
    // would have written ~/.tuxblox/TuxBloxLauncher -- a path that does not
    // exist, i.e. a menu entry that silently does nothing when clicked.
    {
        const std::string bundledExe = (work / "tuxblox" / "launcher" / "TuxBloxLauncher").string();
        createDesktopShortcut(bundledExe);

        assert(fs::exists(menuEntry));
        const std::string content = readFile(menuEntry);
        assert(content.find("Exec=\"" + bundledExe + "\"\n") != std::string::npos);
        // Nothing may reconstruct a path from the install directory any more.
        assert(content.find("Exec=\"" + (work / "tuxblox").string() + "/TuxBloxLauncher\"") == std::string::npos);
        // The rest of the entry must be unchanged.
        assert(content.find("[Desktop Entry]\n") != std::string::npos);
        assert(content.find("Name=TuxBlox Launcher\n") != std::string::npos);
        assert(content.find("Icon=tuxblox\n") != std::string::npos);
        assert(content.find("Categories=Game;\n") != std::string::npos);

        // The icon still goes to the per-user icon theme, not next to the
        // executable -- the path argument's new meaning must not have
        // dragged the icon along with it.
        assert(fs::exists(fakeHome / ".local" / "share" / "icons" / "hicolor" / "256x256" / "apps" / "tuxblox.png"));
        assert(!fs::exists(work / "tuxblox" / "launcher" / "tuxblox.png"));
    }

    // The flat-file shape still works: passing a launcher that lives directly
    // at the install root produces exactly the Exec= line the old
    // installDir-based implementation did, so an older manifest that still
    // ships a bare-binary launcher artifact is unaffected.
    {
        const std::string flatExe = (work / "tuxblox" / "TuxBloxLauncher").string();
        createDesktopShortcut(flatExe);

        const std::string content = readFile(menuEntry);
        assert(content.find("Exec=\"" + flatExe + "\"\n") != std::string::npos);
    }

    // refreshUrlHandlers takes the same value and must agree with the menu
    // entry -- the two are written from one resolved path in app.cpp, and a
    // divergence would mean clicking a roblox:// link and clicking the menu
    // icon launch different (or nonexistent) binaries.
    {
        const std::string bundledExe = (work / "tuxblox" / "launcher" / "TuxBloxLauncher").string();
        createDesktopShortcut(bundledExe);
        refreshUrlHandlers(bundledExe);

        const std::string menu = readFile(menuEntry);
        assert(menu.find("Exec=\"" + bundledExe + "\"\n") != std::string::npos);

        for (const char* id : {"tuxblox-roblox-handler.desktop",
                                "tuxblox-player-handler.desktop",
                                "tuxblox-studio-handler.desktop"}) {
            const fs::path handler = appsDir / id;
            assert(fs::exists(handler));
            const std::string h = readFile(handler);
            assert(h.find("Exec=\"" + bundledExe + "\" %u\n") != std::string::npos);
        }
    }

    fs::remove_all(work);

    printf("desktop_shortcut: all tests passed\n");
    return 0;
}
