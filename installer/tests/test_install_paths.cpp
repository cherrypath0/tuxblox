#include "install_paths.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

int main() {
    using tuxblox::installDir;
    using tuxblox::hasEnoughDiskSpace;

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

    printf("install_paths: all tests passed\n");
    return 0;
}
