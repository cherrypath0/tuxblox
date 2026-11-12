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
#include "FlagManager.h"
#include <algorithm>
#include <cstdint>
#include "roblox_config.h"
#include "user_agent.h"
#include <filesystem>
#include <system_error>

namespace tuxblox {

namespace {

// Long enough for a slow connection, short enough that nobody notices it failing. The
// program has a perfectly good answer without it.
constexpr int kFlagFetchSeconds = 5;

// What the Roblox installer ran with before the number was a flag, and a ceiling that is
// about saturating a connection rather than a machine: past this it is more sockets, not
// more speed, and Roblox's CDN sees a burst it has no reason to welcome.
constexpr int kDefaultInstallerWorkers = 4;
constexpr int kMaxInstallerWorkers = 16;

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

const LauncherFlags& launcherFlags() {
    // Sealed on the first call, which initLauncherFlags() makes deliberately so the wait
    // happens there and not under a window that is trying to draw.
    static const LauncherFlags flags = [] {
        LauncherFlags out;
        out.playerButton = !flagBool("tbxFlagDisablePlayerButton");
        out.christmasTheme = flagBool("tbxFlagEnableChristmasTheme");
        out.geoTool = !flagBool("tbxFlagDisableGeoTool");
        out.crashTelemetry = !flagBool("tbxFlagDisableCrashTelemetry");
        out.robloxNextVersion = flagBool("tbxFlagEnableRobloxNextVersionDownload");
        out.updater = !flagBool("tbxFlagDisableUpdater");
        out.followPromotedBuild =
            flagBool("tbxFlagEnableChannelToUpperChannelUpgradeOnBuildPromotion");
        if (flagBool("tbxFlagEnableParallelWorkersForRobloxInstaller")) {
            const int64_t asked = flagInt("tbxIntRobloxInstallerParallelWorkers");
            // Clamped rather than trusted: this is a thread count, and a set of flags that
            // predates the number would otherwise read as 0 and download nothing at all.
            out.robloxInstallerWorkers =
                asked <= 0 ? kDefaultInstallerWorkers
                           : static_cast<int>(std::min<int64_t>(asked, kMaxInstallerWorkers));
        }
        out.privacyPolicyVersion = flagString("tbxStringPrivacyPolicyVersion");
        out.termsOfUseVersion = flagString("tbxStringTermsOfUseVersion");
        return out;
    }();
    return flags;
}

void initLauncherFlags(const std::string& installDir) {
    const Settings settings = loadSettings(installDir);
    const RecordedRelease release = readRecordedRelease(installDir);
    initFlags(installDir, flagChannelFor(settings, settingsFilePresent(installDir), release),
              release.present,
              kFlagFetchSeconds, {}, configBaseUrl(), userAgent());
}

} // namespace tuxblox
