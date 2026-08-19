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
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static void writeAll(const fs::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << s;
}

int main() {
    using namespace tuxblox;

    assert(sanitizeBridgeLinkName("places") == "places");
    assert(sanitizeBridgeLinkName("my:places?") == "my_places_");
    assert(sanitizeBridgeLinkName("") == "root");
    assert(sanitizeBridgeLinkName("trailing.") == "trailing");
    assert(sanitizeBridgeLinkName("...") == "root");

    const fs::path tmp = fs::temp_directory_path() / "tuxblox_bridge_test";
    fs::remove_all(tmp);
    const fs::path installDir = tmp / "install";
    const fs::path driveC = installDir / "runtime" / "pfx" / "drive_c";
    fs::create_directories(driveC);

    const fs::path bridgeRoot = driveC / "users" / "user" / "Documents" / "TuxBlox Files";

    // A host file outside the prefix gets a directory symlink and a C:\ path.
    const fs::path places = tmp / "places";
    fs::create_directories(places);
    writeAll(places / "map.rbxl", "original");

    const std::string win = bridgeHostPathIntoPrefix(installDir.string(), (places / "map.rbxl").string());
    assert(win == std::string(kBridgeWindowsRoot) + "\\places\\map.rbxl");
    assert(fs::is_symlink(bridgeRoot / "places"));

    // Write-through: writing via the bridged path must land on the ORIGINAL
    // file, which is the whole reason this is a symlink and not a copy.
    writeAll(bridgeRoot / "places" / "map.rbxl", "edited");
    assert(readAll(places / "map.rbxl") == "edited");

    // Rename-into-place (what a save actually does) also lands on the original
    // directory, not inside the prefix.
    writeAll(bridgeRoot / "places" / "map.rbxl.tmp", "saved");
    fs::rename(bridgeRoot / "places" / "map.rbxl.tmp", bridgeRoot / "places" / "map.rbxl");
    assert(readAll(places / "map.rbxl") == "saved");
    assert(!fs::exists(driveC / "users" / "user" / "Documents" / "TuxBlox Files" / "places" / "map.rbxl.tmp"));

    // Idempotent: the same directory reuses its existing link.
    const std::string again = bridgeHostPathIntoPrefix(installDir.string(), (places / "map.rbxl").string());
    assert(again == win);

    // Collision: a DIFFERENT directory with the same basename gets a suffix.
    const fs::path otherPlaces = tmp / "other" / "places";
    fs::create_directories(otherPlaces);
    writeAll(otherPlaces / "two.rbxl", "two");
    const std::string win2 =
        bridgeHostPathIntoPrefix(installDir.string(), (otherPlaces / "two.rbxl").string());
    assert(win2 == std::string(kBridgeWindowsRoot) + "\\places-2\\two.rbxl");
    assert(fs::is_symlink(bridgeRoot / "places-2"));

    // A path already inside drive_c needs no symlink at all.
    const fs::path inPrefix = driveC / "users" / "user" / "Documents";
    fs::create_directories(inPrefix);
    writeAll(inPrefix / "inside.rbxl", "inside");
    const std::string winInside =
        bridgeHostPathIntoPrefix(installDir.string(), (inPrefix / "inside.rbxl").string());
    assert(winInside == "C:\\users\\user\\Documents\\inside.rbxl");

    // Dangling links get pruned; live ones do not.
    const fs::path doomed = tmp / "doomed";
    fs::create_directories(doomed);
    writeAll(doomed / "gone.rbxl", "gone");
    bridgeHostPathIntoPrefix(installDir.string(), (doomed / "gone.rbxl").string());
    assert(fs::is_symlink(bridgeRoot / "doomed"));
    fs::remove_all(doomed);
    bridgeHostPathIntoPrefix(installDir.string(), (places / "map.rbxl").string()); // triggers a sweep
    assert(!fs::exists(fs::symlink_status(bridgeRoot / "doomed")));
    assert(fs::is_symlink(bridgeRoot / "places"));

    // A nonexistent host path fails cleanly rather than inventing a path.
    assert(bridgeHostPathIntoPrefix(installDir.string(), (tmp / "nope.rbxl").string()).empty());

    // INVARIANT: never a host path. Every success above starts with "C:\".
    for (const std::string& s : {win, win2, winInside, again}) {
        assert(s.rfind("C:\\", 0) == 0);
        assert(s.find(tmp.string()) == std::string::npos);
    }

    fs::remove_all(tmp);
    std::printf("prefix_file_bridge: all tests passed\n");
    return 0;
}
