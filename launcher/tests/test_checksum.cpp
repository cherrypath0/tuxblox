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
