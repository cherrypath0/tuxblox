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

#include "prefix/registry.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

namespace {

// A trimmed Wine registry file: two keys of plain values and one holding a
// value long enough that Wine wrapped it over several lines.
const char *pFixture = R"(WINE REGISTRY Version 2
;; All keys relative to \\Machine

#arch=win64

[Software\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}] 1790611128
#time=1dd4f623fe5a31a
"name"="Microsoft Edge WebView2 Runtime"
"pv"="109.0.1518.140"

[Software\\TuxBlox\\Wrapped] 1790611128
#time=1dd4f623fe5a9be
"Before"="keep me"
"Big"=hex:11,22,33,\
  44,55,66,\
  77,88
"After"="keep me too"
)";

std::string readFileText(const fs::path& file) {
    std::ifstream in(file);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

fs::path writeFixture(const fs::path& file) {
    fs::create_directories(file.parent_path());
    std::ofstream(file) << pFixture;
    return file;
}

} // namespace

int main() {
    using namespace tuxblox;

    const std::string clients =
        "Software\\\\Microsoft\\\\EdgeUpdate\\\\Clients\\\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
    const std::string wrapped = "Software\\\\TuxBlox\\\\Wrapped";
    const fs::path base = fs::temp_directory_path() / "tuxblox_test_registry";
    const fs::path file = base / "system.reg";

    // A value that is there goes, and nothing around it moves.
    {
        writeFixture(file);
        assert(removeRegKeyValues(file, clients, {"pv"}));
        assert(getRegValue(file, clients, "pv").empty());
        assert(getRegValue(file, clients, "name") == "\"Microsoft Edge WebView2 Runtime\"");
        assert(getRegValue(file, wrapped, "Before") == "\"keep me\"");
        assert(readFileText(file).find("[" + clients + "]") != std::string::npos);
    }

    // A value that is not there is not an error, and the file is left as it was.
    {
        const std::string before = readFileText(writeFixture(file));
        assert(!removeRegKeyValues(file, clients, {"NoSuchValue"}));
        assert(!removeRegKeyValues(file, "Software\\\\Nowhere", {"pv"}));
        assert(readFileText(file) == before);
    }

    // A wrapped value goes whole, rather than leaving its later lines behind as
    // text nothing can read.
    {
        writeFixture(file);
        assert(removeRegKeyValues(file, wrapped, {"Big"}));
        const std::string text = readFileText(file);
        assert(text.find("hex:11,22,33") == std::string::npos);
        assert(text.find("44,55,66") == std::string::npos);
        assert(text.find("77,88") == std::string::npos);
        assert(getRegValue(file, wrapped, "Before") == "\"keep me\"");
        assert(getRegValue(file, wrapped, "After") == "\"keep me too\"");
    }

    // Several names in one pass.
    {
        writeFixture(file);
        assert(removeRegKeyValues(file, wrapped, {"Before", "Big", "After"}));
        assert(getRegValue(file, wrapped, "Before").empty());
        assert(getRegValue(file, wrapped, "After").empty());
        assert(getRegValue(file, clients, "pv") == "\"109.0.1518.140\"");
    }

    // A file that is not there at all.
    {
        fs::remove_all(base);
        assert(!removeRegKeyValues(file, clients, {"pv"}));
    }

    fs::remove_all(base);
    printf("registry: all tests passed\n");
    return 0;
}
