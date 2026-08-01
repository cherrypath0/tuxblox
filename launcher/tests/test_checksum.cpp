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

#include "checksum.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    using tuxblox::sha256Bytes;
    using tuxblox::sha256File;

    // NIST test vector: sha256("")
    {
        std::string h = sha256Bytes(reinterpret_cast<const unsigned char*>(""), 0);
        assert(h == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    // NIST test vector: sha256("abc")
    {
        const char* abc = "abc";
        std::string h = sha256Bytes(reinterpret_cast<const unsigned char*>(abc), 3);
        assert(h == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    }

    // sha256File must match sha256Bytes for the same content.
    {
        fs::path path = fs::temp_directory_path() / "tuxblox_test_checksum_abc.txt";
        {
            std::ofstream out(path, std::ios::binary);
            out << "abc";
        }
        std::string h = sha256File(path.string());
        assert(h == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        fs::remove(path);
    }

    // Missing file throws.
    {
        bool threw = false;
        try {
            sha256File("/nonexistent/tuxblox_test_missing_file.bin");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }

    printf("checksum: all tests passed\n");
    return 0;
}
