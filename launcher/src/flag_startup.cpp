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

#include "flag_startup.h"
#include "flags.h"
#include "roblox_config.h"
#include "user_agent.h"
#include <filesystem>
#include <system_error>

namespace tuxblox {

namespace {

// Long enough for a slow connection, short enough that nobody notices it failing. The
// program has a perfectly good answer without it.
constexpr int kFlagFetchSeconds = 5;

} // namespace

bool settingsFilePresent(const std::string& installDir) {
    std::error_code ignored;
    return std::filesystem::exists(installDir + "/settings.json", ignored);
}

std::string flagChannelFor(const Settings& settings, bool settingsPresent,
                           const RecordedRelease& release) {
    if (settingsPresent && !settings.channel.empty()) return settings.channel;
    if (release.present && !release.file.channel.empty()) return release.file.channel;
    if (!settings.channel.empty()) return settings.channel;
    return "stable";
}

std::vector<std::string> flagEnvVars(const std::string& installDir) {
    const RecordedRelease release = readRecordedRelease(installDir);
    std::vector<std::string> env = {
        "TUXBLOX_FLAGS_CHANNEL=" +
            flagChannelFor(loadSettings(installDir), settingsFilePresent(installDir), release),
    };
    // Only for an install that records a release, which is what keeps a local build out
    // of build/ from reaching the network on the launch path.
    if (release.present) env.push_back("TUXBLOX_FLAGS_RELEASED=1");
    return env;
}

void initLauncherFlags(const std::string& installDir) {
    const Settings settings = loadSettings(installDir);
    const RecordedRelease release = readRecordedRelease(installDir);
    initFlags(installDir, flagChannelFor(settings, settingsFilePresent(installDir), release),
              release.present,
              kFlagFetchSeconds, {}, configBaseUrl(), userAgent());
}

} // namespace tuxblox
