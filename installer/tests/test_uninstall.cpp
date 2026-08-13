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
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& p) {
    std::ifstream in(p);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

int main() {
    using namespace tuxblox;

    // removeDesktopFiles: removes every tuxblox-*.desktop entry, leaves
    // unrelated files (including a differently-prefixed .desktop and a
    // tuxblox-prefixed non-.desktop file) alone.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_uninstall_desktop_files";
        fs::remove_all(dir);
        fs::create_directories(dir);
        std::ofstream(dir / "tuxblox-launcher.desktop") << "x";
        std::ofstream(dir / "tuxblox-url-handler.desktop") << "x";
        std::ofstream(dir / "tuxblox-player.desktop") << "x";
        std::ofstream(dir / "steam.desktop") << "x";
        std::ofstream(dir / "tuxblox-notes.txt") << "x";

        removeDesktopFiles(dir.string());

        assert(!fs::exists(dir / "tuxblox-launcher.desktop"));
        assert(!fs::exists(dir / "tuxblox-url-handler.desktop"));
        assert(!fs::exists(dir / "tuxblox-player.desktop"));
        assert(fs::exists(dir / "steam.desktop"));
        assert(fs::exists(dir / "tuxblox-notes.txt"));

        fs::remove_all(dir);
    }

    // removeDesktopFiles on a directory that doesn't exist -- no-op, no throw.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_uninstall_no_such_dir";
        fs::remove_all(dir);
        removeDesktopFiles(dir.string());
        assert(!fs::exists(dir));
    }

    // stripMimeappsAssociations: drops lines referencing the url handler,
    // keeps everything else, preserves line order.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_uninstall_mimeapps";
        fs::remove_all(dir);
        fs::create_directories(dir);
        fs::path mimeapps = dir / "mimeapps.list";
        std::ofstream(mimeapps) <<
            "[Default Applications]\n"
            "x-scheme-handler/roblox-player=tuxblox-url-handler.desktop\n"
            "x-scheme-handler/http=firefox.desktop\n"
            "x-scheme-handler/roblox-studio=tuxblox-url-handler.desktop;\n"
            "[Added Associations]\n"
            "text/plain=gedit.desktop;\n";

        stripMimeappsAssociations(mimeapps.string());

        std::string result = readFile(mimeapps);
        assert(result.find("tuxblox-url-handler.desktop") == std::string::npos);
        assert(result.find("x-scheme-handler/http=firefox.desktop") != std::string::npos);
        assert(result.find("text/plain=gedit.desktop") != std::string::npos);
        assert(result.find("[Default Applications]") != std::string::npos);
        assert(result.find("[Added Associations]") != std::string::npos);

        fs::remove_all(dir);
    }

    // stripMimeappsAssociations on a missing file -- no-op, no throw.
    {
        fs::path missing = fs::temp_directory_path() / "tuxblox_test_uninstall_no_such_mimeapps.list";
        fs::remove(missing);
        stripMimeappsAssociations(missing.string());
        assert(!fs::exists(missing));
    }

    // removeInstallDir: removes an existing directory tree and reports
    // success; reports success (not failure) for an already-missing one.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_uninstall_install_dir";
        fs::remove_all(dir);
        fs::create_directories(dir / "ProtonBuild" / "dist");
        std::ofstream(dir / "runtime_marker") << "x";

        assert(removeInstallDir(dir.string()) == true);
        assert(!fs::exists(dir));

        assert(removeInstallDir(dir.string()) == true); // already gone -- still success
    }

    // removeIconFile: removes an existing file; no-op (no throw) for a
    // missing one.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_uninstall_icon";
        fs::remove_all(dir);
        fs::create_directories(dir);
        fs::path icon = dir / "tuxblox.png";
        std::ofstream(icon) << "x";
        assert(fs::exists(icon));

        removeIconFile(icon.string());
        assert(!fs::exists(icon));

        removeIconFile(icon.string()); // already gone -- still no-op, no throw

        fs::remove_all(dir);
    }

    printf("uninstall: all tests passed\n");
    return 0;
}
