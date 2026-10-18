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

    // applyFastFlags() reaches every installed version of the target, so a
    // flag the user deleted stops applying in all of them rather than only
    // in whichever one happens to be pinned active.
    {
        const fs::path install = tmp / "install";
        const fs::path versions =
            install / "runtime/pfx/drive_c/users/tester/AppData/Local/Roblox/Versions";
        const fs::path playerA = versions / "version-playerA";
        const fs::path playerB = versions / "version-playerB";
        const fs::path studio = versions / "version-studio";
        for (const fs::path& dir : {playerA, playerB, studio}) fs::create_directories(dir);
        std::ofstream(playerA / targetExeName(LaunchTarget::Player), std::ios::binary) << "exe";
        std::ofstream(playerB / targetExeName(LaunchTarget::Player), std::ios::binary) << "exe";
        std::ofstream(studio / targetExeName(LaunchTarget::Studio), std::ios::binary) << "exe";

        const fs::path playerAFile = playerA / "ClientSettings" / "ClientAppSettings.json";
        const fs::path playerBFile = playerB / "ClientSettings" / "ClientAppSettings.json";
        const fs::path studioFile = studio / "ClientSettings" / "ClientAppSettings.json";

        Settings settings;
        settings.fastFlags.player = {{"PlayerFlag", "1"}};
        settings.fastFlags.studio = {{"StudioFlag", "2"}};

        applyFastFlags(install.string(), LaunchTarget::Player, settings);
        assert(nlohmann::json::parse(readFile(playerAFile))["PlayerFlag"] == "1");
        assert(nlohmann::json::parse(readFile(playerBFile))["PlayerFlag"] == "1");
        // Player and Studio share one Versions directory, so the Player's
        // flags must not land in a Studio version.
        assert(!fs::exists(studioFile));

        applyFastFlags(install.string(), LaunchTarget::Studio, settings);
        assert(nlohmann::json::parse(readFile(studioFile))["StudioFlag"] == "2");
        assert(!nlohmann::json::parse(readFile(playerAFile)).contains("StudioFlag"));

        // The point of the whole thing: deleting the last flag has to clear
        // every version, not just the active one.
        settings.fastFlags.player.clear();
        applyFastFlags(install.string(), LaunchTarget::Player, settings);
        assert(!fs::exists(playerAFile));
        assert(!fs::exists(playerBFile));
        assert(fs::exists(studioFile));
    }

    // A prefix with no Versions directory at all (a fresh install) is not an
    // error -- there is simply nothing to write to yet.
    applyFastFlags((tmp / "empty-install").string(), LaunchTarget::Player, Settings{});

    // "automatic" is the default and must write nothing at all, so the engine
    // and the FastFlags page are left to decide between them.
    {
        assert(graphicsFlags("automatic").empty());
        assert(graphicsFlags("").empty());
        assert(graphicsFlags("nonsense").empty());
        assert(withGraphicsChoice({{"AFlag", "1"}}, "automatic").size() == 1);
    }

    // Choosing a renderer asks for it AND rules the other two out, rather than
    // only naming the one wanted.
    {
        auto j = nlohmann::json::parse(renderClientAppSettings(graphicsFlags("vulkan")));
        assert(j.size() == 3);
        assert(j["FFlagDebugGraphicsPreferVulkan"] == "True");
        assert(j["FFlagDebugGraphicsPreferD3D11"] == "False");
        assert(j["FFlagDebugGraphicsPreferOpenGL"] == "False");

        assert(nlohmann::json::parse(renderClientAppSettings(graphicsFlags("d3d11")))
                   ["FFlagDebugGraphicsPreferD3D11"] == "True");
        assert(nlohmann::json::parse(renderClientAppSettings(graphicsFlags("opengl")))
                   ["FFlagDebugGraphicsPreferOpenGL"] == "True");
    }

    // The setting beats the FastFlags page wherever the two name the same flag,
    // and leaves every other row the user wrote alone.
    {
        const std::vector<FastFlag> typed = {{"FFlagDebugGraphicsPreferOpenGL", "True"},
                                              {"DFIntTaskSchedulerTargetFps", "60"}};
        auto j = nlohmann::json::parse(renderClientAppSettings(withGraphicsChoice(typed, "vulkan")));
        assert(j["FFlagDebugGraphicsPreferOpenGL"] == "False");
        assert(j["FFlagDebugGraphicsPreferVulkan"] == "True");
        assert(j["DFIntTaskSchedulerTargetFps"] == "60");
    }

    // A name the user typed twice has to be overwritten in both rows: the last
    // one wins when the file is written, so leaving one behind would let it win.
    {
        const std::vector<FastFlag> typed = {{"FFlagDebugGraphicsPreferVulkan", "True"},
                                              {"FFlagDebugGraphicsPreferVulkan", "True"}};
        for (const FastFlag& flag : withGraphicsChoice(typed, "opengl")) {
            if (flag.name == "FFlagDebugGraphicsPreferVulkan") assert(flag.value == "False");
        }
        auto j = nlohmann::json::parse(renderClientAppSettings(withGraphicsChoice(typed, "opengl")));
        assert(j["FFlagDebugGraphicsPreferVulkan"] == "False");
        assert(j["FFlagDebugGraphicsPreferOpenGL"] == "True");
    }

    // applyFastFlags() carries the setting through to the file on disk, for both
    // apps, since the renderer is one choice rather than one per app.
    {
        const fs::path install = tmp / "graphics-install";
        const fs::path versions =
            install / "runtime/pfx/drive_c/users/tester/AppData/Local/Roblox/Versions";
        const fs::path player = versions / "version-p";
        const fs::path studio = versions / "version-s";
        fs::create_directories(player);
        fs::create_directories(studio);
        std::ofstream(player / targetExeName(LaunchTarget::Player), std::ios::binary) << "exe";
        std::ofstream(studio / targetExeName(LaunchTarget::Studio), std::ios::binary) << "exe";

        Settings settings;
        settings.graphics = "d3d11";
        applyFastFlags(install.string(), LaunchTarget::Player, settings);
        applyFastFlags(install.string(), LaunchTarget::Studio, settings);
        for (const fs::path& dir : {player, studio}) {
            auto j = nlohmann::json::parse(readFile(dir / "ClientSettings" / "ClientAppSettings.json"));
            assert(j["FFlagDebugGraphicsPreferD3D11"] == "True");
            assert(j["FFlagDebugGraphicsPreferVulkan"] == "False");
        }

        // Back to automatic with no flags of the user's own: the override file
        // goes away entirely rather than being left saying False three times.
        settings.graphics = "automatic";
        applyFastFlags(install.string(), LaunchTarget::Player, settings);
        assert(!fs::exists(player / "ClientSettings" / "ClientAppSettings.json"));
    }

    fs::remove_all(tmp);
    std::printf("fastflag_file: all tests passed\n");
    return 0;
}
