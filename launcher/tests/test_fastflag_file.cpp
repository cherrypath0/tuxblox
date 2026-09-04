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

#include "fastflag_file.h"
#include "json.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    using namespace tuxblox;

    // Every value is written as a JSON string, whatever it looks like --
    // Roblox accepts that for every flag type, and it means the editor never
    // has to guess whether "60" is a number or a name.
    {
        std::string out = renderClientAppSettings({{"DFIntTaskSchedulerTargetFps", "60"},
                                                    {"FFlagDebugGraphicsPreferVulkan", "True"}});
        auto j = nlohmann::json::parse(out);
        assert(j["DFIntTaskSchedulerTargetFps"] == "60");
        assert(j["FFlagDebugGraphicsPreferVulkan"] == "True");
        assert(j.size() == 2);
    }

    // No flags is a valid, empty settings object, not malformed JSON.
    {
        auto j = nlohmann::json::parse(renderClientAppSettings({}));
        assert(j.is_object());
        assert(j.empty());
    }

    // Rows are written in the order the editor shows them, so the file is
    // stable between launches and readable in a diff.
    {
        std::string out = renderClientAppSettings({{"BFlag", "1"}, {"AFlag", "2"}, {"CFlag", "3"}});
        assert(out.find("BFlag") < out.find("AFlag"));
        assert(out.find("AFlag") < out.find("CFlag"));
    }

    // A duplicate name collapses to one key with the last value -- a JSON
    // object could not hold both anyway.
    {
        auto j = nlohmann::json::parse(renderClientAppSettings({{"Dupe", "first"}, {"Dupe", "last"}}));
        assert(j.size() == 1);
        assert(j["Dupe"] == "last");
    }

    // Names and values that contain JSON metacharacters must not be able to
    // produce a file Roblox cannot parse.
    {
        auto j = nlohmann::json::parse(
            renderClientAppSettings({{"Quote\"Flag", "back\\slash"}, {"New\nLine", "tab\there"}}));
        assert(j["Quote\"Flag"] == "back\\slash");
        assert(j["New\nLine"] == "tab\there");
    }

    const fs::path tmp = fs::temp_directory_path() / "tuxblox_fastflag_file_test";
    fs::remove_all(tmp);
    const fs::path versionDir = tmp / "version-abc123";
    fs::create_directories(versionDir);
    const fs::path written = versionDir / "ClientSettings" / "ClientAppSettings.json";

    // The ClientSettings directory does not exist in a fresh version folder,
    // so writing has to create it.
    {
        assert(writeClientAppSettings(versionDir.string(), {{"AFlag", "1"}}));
        assert(fs::exists(written));
        auto j = nlohmann::json::parse(readFile(written));
        assert(j["AFlag"] == "1");
    }

    // A second write replaces the file rather than merging into it -- the
    // editor is the only source of truth for what is in there.
    {
        assert(writeClientAppSettings(versionDir.string(), {{"BFlag", "2"}}));
        auto j = nlohmann::json::parse(readFile(written));
        assert(j.size() == 1);
        assert(j.contains("BFlag"));
        assert(!j.contains("AFlag"));
    }

    // Clearing every flag removes the file, so the client goes back to stock
    // behaviour instead of reading an empty override.
    {
        assert(writeClientAppSettings(versionDir.string(), {}));
        assert(!fs::exists(written));
    }

    // Removing when there is nothing to remove is success, not failure.
    assert(writeClientAppSettings(versionDir.string(), {}));

    // A version directory that is not a directory at all is reported as a
    // failure rather than crashing or silently doing nothing.
    {
        const fs::path notADir = tmp / "regular-file";
        std::ofstream(notADir, std::ios::binary) << "not a directory";
        assert(!writeClientAppSettings(notADir.string(), {{"AFlag", "1"}}));
    }

    fs::remove_all(tmp);
    std::printf("fastflag_file: all tests passed\n");
    return 0;
}
