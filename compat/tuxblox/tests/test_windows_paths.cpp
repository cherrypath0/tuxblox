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

#include "../src/support/windows_paths.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace {

int Failures = 0;

void expect(const std::string& value, const std::string& want) {
    const std::string got = tuxblox::canonicalWindowsPath(value);
    if (got == want) {
        return;
    }
    Failures++;
    std::printf("  FAIL: %s\n     got: %s\n    want: %s\n",
            value.c_str(), got.c_str(), want.c_str());
}

} // namespace

int main() {
    // Registry values arrive as the file spells them: quoted, backslashes doubled.
    expect("\"C:\\\\users\\\\cherry\"", "\"C:\\\\Users\\\\cherry\"");
    expect("\"C:\\\\users\\\\cherry\\\\AppData\\\\Local\"", "\"C:\\\\Users\\\\cherry\\\\AppData\\\\Local\"");
    expect("\"C:\\\\users\\\\Public\\\\Desktop\"", "\"C:\\\\Users\\\\Public\\\\Desktop\"");
    expect("\"C:\\\\users\"", "\"C:\\\\Users\"");

    expect("\"C:\\\\windows\"", "\"C:\\\\Windows\"");
    expect("\"C:\\\\windows\\\\system32\"", "\"C:\\\\Windows\\\\System32\"");
    expect("\"C:\\\\windows\\\\system32\\\\drivers\"", "\"C:\\\\Windows\\\\System32\\\\drivers\"");
    expect("\"C:\\\\windows\\\\syswow64\"", "\"C:\\\\Windows\\\\SysWOW64\"");
    expect("\"C:\\\\programdata\\\\Roblox\"", "\"C:\\\\ProgramData\\\\Roblox\"");
    expect("\"C:\\\\program files (x86)\\\\Roblox\"", "\"C:\\\\Program Files (x86)\\\\Roblox\"");
    expect("\"C:\\\\program files\\\\Roblox\"", "\"C:\\\\Program Files\\\\Roblox\"");

    // Already correct, and a drive other than C
    expect("\"C:\\\\Users\\\\cherry\"", "\"C:\\\\Users\\\\cherry\"");
    expect("\"D:\\\\windows\"", "\"D:\\\\Windows\"");

    // Only the component under the drive root, and only at its own level
    expect("\"C:\\\\users\\\\cherry\\\\windows\\\\system32\"",
            "\"C:\\\\Users\\\\cherry\\\\windows\\\\system32\"");
    expect("\"C:\\\\system32\"", "\"C:\\\\system32\"");
    expect("\"C:\\\\games\\\\windows\"", "\"C:\\\\games\\\\windows\"");

    // A longer name that merely starts with a known one must not match
    expect("\"C:\\\\usersdata\\\\x\"", "\"C:\\\\usersdata\\\\x\"");
    expect("\"C:\\\\windowsapps\"", "\"C:\\\\windowsapps\"");

    // Anything that is not a quoted drive path is left exactly as it was
    expect("dword:00000000", "dword:00000000");
    expect("\"Console\"", "\"Console\"");
    expect("\"\\\\\\\\ARCH\"", "\"\\\\\\\\ARCH\"");
    expect("\"%USERPROFILE%\\\\AppData\"", "\"%USERPROFILE%\\\\AppData\"");
    expect("\"\"", "\"\"");
    expect("", "");
    expect("\"C:\"", "\"C:\"");
    expect("\"C:\\\\\"", "\"C:\\\\\"");

    // The forms a .reg file actually holds: a default value, a typed value, and
    // a path quoted inside a command line.
    expect("@=\"C:\\\\users\\\\cherry\\\\RobloxStudioBeta.exe\"",
            "@=\"C:\\\\Users\\\\cherry\\\\RobloxStudioBeta.exe\"");
    expect("str(2):\"C:\\\\users\"", "str(2):\"C:\\\\Users\"");
    expect("@=\"\\\"C:\\\\users\\\\cherry\\\\x.exe\\\" %1\"",
            "@=\"\\\"C:\\\\Users\\\\cherry\\\\x.exe\\\" %1\"");

    // A drive letter has to stand on its own, not sit inside a word
    expect("dword:00000000", "dword:00000000");
    expect("\"abc:\\\\users\"", "\"abc:\\\\users\"");

    // Two paths on one line are both corrected
    expect("\"C:\\\\users\\\\a;C:\\\\windows\\\\system32\"",
            "\"C:\\\\Users\\\\a;C:\\\\Windows\\\\System32\"");

    // A whole registry file, which is how the drive actually holds these
    const fs::path base = fs::temp_directory_path() / "tuxblox_test_windows_paths";
    fs::remove_all(base);
    fs::create_directories(base);
    const fs::path reg = base / "user.reg";

    const std::string before =
            "WINE REGISTRY Version 2\n\n"
            "[Environment] 1792965369\n"
            "\"TEMP\"=\"C:\\\\users\\\\cherry\\\\AppData\\\\Local\\\\Temp\"\n"
            "\"CLIENTNAME\"=\"Console\"\n\n"
            "[Software\\\\Classes\\\\roblox-studio\\\\DefaultIcon] 1793012537\n"
            "@=\"C:\\\\users\\\\cherry\\\\x.exe\"\n\n"
            "[Software\\\\Microsoft\\\\Windows NT\\\\CurrentVersion\\\\ProfileList] 1792965372\n"
            "\"ProfilesDirectory\"=str(2):\"C:\\\\users\"\n";
    std::ofstream(reg) << before;

    if (!tuxblox::canonicaliseRegFile(reg)) {
        Failures++;
        std::printf("  FAIL: canonicaliseRegFile reported no change\n");
    }

    std::ifstream in(reg);
    const std::string after((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (after.find("C:\\\\users") != std::string::npos) {
        Failures++;
        std::printf("  FAIL: a lower-case path survived the rewrite\n");
    }
    if (after.find("\"CLIENTNAME\"=\"Console\"") == std::string::npos ||
            after.find("WINE REGISTRY Version 2") == std::string::npos ||
            after.find("[Environment] 1792965369") == std::string::npos) {
        Failures++;
        std::printf("  FAIL: the rewrite did not leave the rest of the file alone\n");
    }

    // A file with nothing to correct is left untouched, so launches stop rewriting it
    if (tuxblox::canonicaliseRegFile(reg)) {
        Failures++;
        std::printf("  FAIL: a second pass rewrote a file that was already correct\n");
    }
    fs::remove_all(base);

    if (Failures > 0) {
        std::printf("windows_paths: %d failed\n", Failures);
        return 1;
    }
    std::printf("windows_paths: all tests passed\n");
    return 0;
}
