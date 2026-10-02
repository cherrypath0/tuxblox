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

#include "pe_version.h"
#include "pe_fixture.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

using tuxblox_test::minimalPe;
using tuxblox_test::versionResourceWith;

std::string writeTemp(const std::string& name, const std::string& bytes) {
    const std::string path = (fs::temp_directory_path() / name).string();
    std::ofstream f(path, std::ios::binary);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    f.close();
    return path;
}

} // namespace

int main() {
    using namespace tuxblox;

    // Roblox writes the version with comma separators; a version number is read with dots.
    assert(normalizeVersionString("0, 740, 0, 7400927") == "0.740.0.7400927");
    assert(normalizeVersionString("0,740,0,7400927") == "0.740.0.7400927");
    assert(normalizeVersionString("0.740.0.7400927") == "0.740.0.7400927");
    assert(normalizeVersionString("  0, 740, 0, 7400927  ") == "0.740.0.7400927");

    // Anything that is not a version number is refused rather than passed through half-formatted.
    assert(normalizeVersionString("").empty());
    assert(normalizeVersionString("not a version").empty());
    assert(normalizeVersionString("0.740").empty());
    assert(normalizeVersionString("0.740.0.7400927.5").empty());
    assert(normalizeVersionString("0..0.1").empty());

    // The value that follows the FileVersion key is the one wanted.
    assert(fileVersionFromResource(versionResourceWith("FileVersion", "0, 740, 0, 7400927")) ==
           "0.740.0.7400927");

    // A resource with no FileVersion key at all yields nothing.
    assert(fileVersionFromResource(versionResourceWith("ProductName", "Roblox")).empty());

    // A key that merely ends in FileVersion is a different key, and must not be mistaken for it.
    assert(fileVersionFromResource(versionResourceWith("AssemblyFileVersion", "9, 9, 9, 9")).empty());

    // Truncated and empty resources are answered, not crashed on.
    assert(fileVersionFromResource("").empty());
    {
        std::string blob = versionResourceWith("FileVersion", "0, 740, 0, 7400927");
        assert(fileVersionFromResource(blob.substr(0, blob.size() / 2)).empty());
    }

    // The whole walk, from a file on disk through the resource tree to the value.
    {
        const std::string path = writeTemp("tuxblox_test_pe_version.exe",
                                           minimalPe(versionResourceWith("FileVersion", "0, 740, 0, 7400927")));
        assert(peFileVersion(path) == "0.740.0.7400927");
        fs::remove(path);
    }

    // A file whose resource tree holds no version resource is not an error, just no answer.
    {
        const std::string path = writeTemp("tuxblox_test_pe_noversion.exe",
                                           minimalPe(versionResourceWith("FileVersion", "1, 2, 3, 4"), 3));
        assert(peFileVersion(path).empty());
        fs::remove(path);
    }

    // Nothing here may throw or crash on a file that is not a Windows program at all.
    assert(peFileVersion("/nonexistent/tuxblox/does/not/exist.exe").empty());
    {
        const std::string path = writeTemp("tuxblox_test_pe_garbage.exe", std::string(4096, '\x7F'));
        assert(peFileVersion(path).empty());
        fs::remove(path);
    }
    {
        const std::string path =
            writeTemp("tuxblox_test_pe_truncated.exe",
                      minimalPe(versionResourceWith("FileVersion", "0, 1, 2, 3")).substr(0, 300));
        assert(peFileVersion(path).empty());
        fs::remove(path);
    }

    printf("pe_version: all tests passed\n");
    return 0;
}
