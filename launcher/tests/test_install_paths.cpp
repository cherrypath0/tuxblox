#include "install_paths.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    setenv("HOME", "/tmp/tuxblox_test_home", 1);
    assert(installDir() == "/tmp/tuxblox_test_home/.local/share/tuxblox");

    unsetenv("HOME");
    bool threw = false;
    try {
        installDir();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    setenv("HOME", "/tmp/tuxblox_test_home", 1); // restore for anything running after

    assert(hasEnoughDiskSpace("/tmp", 1) == true);
    assert(hasEnoughDiskSpace("/tmp", (uint64_t)1 << 60) == false);

    assert(protonBuildDirUnder("/x/tuxblox") == "/x/tuxblox/ProtonBuild");
    assert(protonVersionFilePathUnder("/x/tuxblox") == "/x/tuxblox/ProtonBuild/dist/version");

    // Missing version file -> nullopt.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_missing";
        fs::remove_all(dir);
        auto v = readInstalledProtonVersion(dir.string());
        assert(!v.has_value());
    }

    // Well-formed "<epoch> <version>" file.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_ok";
        fs::remove_all(dir);
        fs::create_directories(dir / "ProtonBuild" / "dist");
        {
            std::ofstream out(dir / "ProtonBuild" / "dist" / "version");
            out << "1753700000 0.1.0";
        }
        auto v = readInstalledProtonVersion(dir.string());
        assert(v.has_value() && *v == "0.1.0");
        fs::remove_all(dir);
    }

    // Malformed file (single token, no version) -> nullopt.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_malformed";
        fs::remove_all(dir);
        fs::create_directories(dir / "ProtonBuild" / "dist");
        {
            std::ofstream out(dir / "ProtonBuild" / "dist" / "version");
            out << "justonetoken";
        }
        auto v = readInstalledProtonVersion(dir.string());
        assert(!v.has_value());
        fs::remove_all(dir);
    }

    printf("install_paths: all tests passed\n");
    return 0;
}
