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

#include "license_file.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    // Missing LICENSE -> created with the embedded license text.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_license_file_missing";
        fs::remove_all(dir);
        fs::create_directories(dir);

        ensureLicenseFile(dir.string());

        fs::path licensePath = dir / "LICENSE";
        assert(fs::exists(licensePath));
        std::ifstream in(licensePath);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(!content.empty());

        fs::remove_all(dir);
    }

    // Existing LICENSE -> left untouched, never overwritten.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_license_file_existing";
        fs::remove_all(dir);
        fs::create_directories(dir);

        fs::path licensePath = dir / "LICENSE";
        {
            std::ofstream out(licensePath);
            out << "pre-existing content, must not be clobbered";
        }

        ensureLicenseFile(dir.string());

        std::ifstream in(licensePath);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content == "pre-existing content, must not be clobbered");

        fs::remove_all(dir);
    }

    // Missing installDir -> best-effort no-op, must not throw.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_license_file_no_such_dir";
        fs::remove_all(dir);
        ensureLicenseFile(dir.string());
        assert(!fs::exists(dir));
    }

    printf("license_file: all tests passed\n");
    return 0;
}
