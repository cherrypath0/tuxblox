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

#include "lnk_resolver.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utime.h>
#include <vector>

namespace fs = std::filesystem;

static std::vector<unsigned char> asciiBytes(const std::string& s) {
    return std::vector<unsigned char>(s.begin(), s.end());
}

static std::vector<unsigned char> utf16LeBytes(const std::string& s) {
    std::vector<unsigned char> out;
    for (char c : s) {
        out.push_back(static_cast<unsigned char>(c));
        out.push_back(0x00);
    }
    return out;
}

int main() {
    using namespace tuxblox;

    assert(std::string(targetExeName(LaunchTarget::Player)) == "RobloxPlayerBeta.exe");
    assert(std::string(targetExeName(LaunchTarget::Studio)) == "RobloxStudioBeta.exe");
    // No drive on disk: the shortcut is looked for under the old account
    // folder name, which is what a drive built before the rename has.
    assert(targetLnkRelPath(LaunchTarget::Player, "/nowhere/drive_c") ==
           "users/user/Desktop/Roblox Player.lnk");
    assert(targetLnkRelPath(LaunchTarget::Studio, "/nowhere/drive_c") ==
           "users/user/Desktop/Roblox Studio.lnk");

    // With one, the shortcut is looked for under the real account folder.
    {
        const fs::path driveC = fs::temp_directory_path() / "tuxblox_test_lnk_account/drive_c";
        fs::remove_all(driveC.parent_path());
        fs::create_directories(driveC / "users/cherry");
        assert(targetLnkRelPath(LaunchTarget::Studio, driveC.string()) ==
               "users/cherry/Desktop/Roblox Studio.lnk");
        fs::remove_all(driveC.parent_path());
    }

    // ASCII-encoded path inside otherwise-binary .lnk bytes.
    {
        std::vector<unsigned char> bytes = {0x01, 0x02, 0x00, 0x03};
        auto ascii = asciiBytes("C:\\Program Files (x86)\\Roblox\\Versions\\version-abc\\RobloxPlayerBeta.exe");
        bytes.insert(bytes.end(), ascii.begin(), ascii.end());
        bytes.push_back(0x00);

        std::string result = extractExeRelPathFromLnkBytes(bytes, "RobloxPlayerBeta.exe");
        assert(result == "/Program Files (x86)/Roblox/Versions/version-abc/RobloxPlayerBeta.exe");
    }

    // UTF-16LE-encoded path (the .lnk format's native encoding).
    {
        std::vector<unsigned char> bytes = {0x01, 0x02, 0x00, 0x03};
        auto wide = utf16LeBytes("D:\\Roblox\\Versions\\version-xyz\\RobloxStudioBeta.exe");
        bytes.insert(bytes.end(), wide.begin(), wide.end());

        std::string result = extractExeRelPathFromLnkBytes(bytes, "RobloxStudioBeta.exe");
        assert(result == "/Roblox/Versions/version-xyz/RobloxStudioBeta.exe");
    }

    // No match in either encoding.
    {
        std::vector<unsigned char> bytes = {0x01, 0x02, 0x03, 0x04};
        std::string result = extractExeRelPathFromLnkBytes(bytes, "RobloxPlayerBeta.exe");
        assert(result.empty());
    }

    // resolveExePath: .lnk points at a real file under a fixture drive_c tree.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_lnk_resolver";
        fs::remove_all(root);
        fs::path driveC = root / "drive_c";
        fs::path exeDir = driveC / "Program Files (x86)" / "Roblox" / "Versions" / "version-abc";
        fs::create_directories(exeDir);
        {
            std::ofstream out(exeDir / "RobloxPlayerBeta.exe", std::ios::binary);
            out << "fake exe";
        }

        fs::path lnkDir = driveC / "users" / "user" / "Desktop";
        fs::create_directories(lnkDir);
        {
            std::ofstream out(lnkDir / "Roblox Player.lnk", std::ios::binary);
            auto ascii = asciiBytes("C:\\Program Files (x86)\\Roblox\\Versions\\version-abc\\RobloxPlayerBeta.exe");
            out.write(reinterpret_cast<const char*>(ascii.data()), static_cast<std::streamsize>(ascii.size()));
        }

        std::string resolved = resolveExePath(LaunchTarget::Player, driveC.string());
        assert(resolved == (exeDir / "RobloxPlayerBeta.exe").string());

        fs::remove_all(root);
    }

    // resolveExePath: no .lnk, falls back to recursive filename search,
    // skipping any path containing "Installer".
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_lnk_resolver_fallback";
        fs::remove_all(root);
        fs::path driveC = root / "drive_c";
        fs::path installerDir = driveC / "Installer";
        fs::path realDir = driveC / "Roblox" / "Versions" / "version-def";
        fs::create_directories(installerDir);
        fs::create_directories(realDir);
        {
            std::ofstream out(installerDir / "RobloxStudioBeta.exe", std::ios::binary);
            out << "decoy";
        }
        {
            std::ofstream out(realDir / "RobloxStudioBeta.exe", std::ios::binary);
            out << "real";
        }

        std::string resolved = resolveExePath(LaunchTarget::Studio, driveC.string());
        assert(resolved == (realDir / "RobloxStudioBeta.exe").string());

        fs::remove_all(root);
    }

    // resolveExePath: nothing found at all -> empty string.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_lnk_resolver_empty";
        fs::remove_all(root);
        fs::create_directories(root / "drive_c");
        std::string resolved = resolveExePath(LaunchTarget::Player, (root / "drive_c").string());
        assert(resolved.empty());
        fs::remove_all(root);
    }

    // Roblox leaves every version it has ever installed in the drive, and TuxBlox installs its files
    // directly so there is no shortcut naming the current one. Without a rule, the search returned
    // whichever the directory happened to yield first, which is how a three-week-old Studio got
    // launched on a machine with four installed.
    {
        const fs::path root = fs::temp_directory_path() / "tuxblox_test_newest_version";
        fs::remove_all(root);
        const fs::path versions = root / "users" / "cherry" / "AppData" / "Local" / "Roblox" / "Versions";

        const std::pair<const char *, std::time_t> builds[] = {
            {"version-aaaaaaaaaaaaaaaa", 1757000000},  // oldest
            {"version-cccccccccccccccc", 1759000000},  // newest
            {"version-bbbbbbbbbbbbbbbb", 1758000000},
        };
        for (const auto& [folder, when] : builds) {
            fs::create_directories(versions / folder);
            const fs::path exe = versions / folder / "RobloxStudioBeta.exe";
            { std::ofstream out(exe); out << "x"; }
            struct utimbuf times{when, when};
            utime(exe.c_str(), &times);
        }

        const std::string found = tuxblox::resolveExePath(tuxblox::LaunchTarget::Studio, root.string());
        assert(found.find("version-cccccccccccccccc") != std::string::npos);

        // An installer that happens to carry the same name is not the program.
        fs::create_directories(versions / "Installer");
        const fs::path fake = versions / "Installer" / "RobloxStudioBeta.exe";
        { std::ofstream out(fake); out << "x"; }
        struct utimbuf later{1760000000, 1760000000};
        utime(fake.c_str(), &later);
        assert(tuxblox::resolveExePath(tuxblox::LaunchTarget::Studio, root.string())
                   .find("version-cccccccccccccccc") != std::string::npos);

        fs::remove_all(root);
    }

    printf("lnk_resolver: all tests passed\n");
    return 0;
}
