#include "lnk_resolver.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
    assert(std::string(targetLnkRelPath(LaunchTarget::Player)) ==
           "users/steamuser/Desktop/Roblox Player.lnk");
    assert(std::string(targetLnkRelPath(LaunchTarget::Studio)) ==
           "users/steamuser/Desktop/Roblox Studio.lnk");

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

        fs::path lnkDir = driveC / "users" / "steamuser" / "Desktop";
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

    printf("lnk_resolver: all tests passed\n");
    return 0;
}
