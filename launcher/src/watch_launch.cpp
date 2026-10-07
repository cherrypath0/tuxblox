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

#include "watch_launch.h"
#include "crash_report.h"
#include "discord_rpc.h"
#include "install_paths.h"
#include "process_launcher.h"
#include "process_name.h"
#include "roblox_autoupdate.h"
#include "roblox_log_capture.h"
#include "roblox_place_info.h"
#include "roblox_universe.h"
#include "settings.h"
#include "bloxstrap_rpc.h"
#include "player_presence.h"
#include "server_region.h"
#include "studio_presence.h"
#include "system_info.h"
#include "ui_adw/message_box.h"
#include "fastflag_file.h"
#include "versions_manifest.h"
#include "wine_shortcut_export.h"
#include <chrono>
#include <filesystem>
#include <ctime>
#include <map>
#include <vector>
#include <memory>
#include <thread>

namespace tuxblox {

// What Roblox told us about a place, asked for once and kept for the session.
struct ResolvedPlace {
    std::string name;
    std::string iconUrl;
    // How the experience's author is credited. Asked for only on a Player session, because Studio shows the place a builder opened rather than an experience somebody is playing.
    std::string creatorLine;
};

// The Discord applications this presence is published under, registered at discord.com/developers. Public, not secrets. Each program reports under its own, because Discord shows the application's own name above the activity and nothing sent over the socket can change it.
// An asset a game named is described by Roblox, not served by it, so it has to be looked up before Discord can be given anything -- once per asset, failures remembered too, or a game rotating its art would mean a request every tick.
void resolveRpcImage(RpcImage& image, std::map<std::string, std::string>& resolved) {
    if (image.assetId.state != RpcField::State::Set || image.assetId.value.empty()) {
        return;
    }
    auto known = resolved.find(image.assetId.value);
    if (known == resolved.end()) {
        known = resolved
                    .emplace(image.assetId.value,
                             fetchThumbnailImageUrl(rpcAssetLookupUrl(image.assetId.value)))
                    .first;
    }
    image.resolvedUrl = known->second;
}

const char kStudioApplicationId[] = "1554221498925973584";
const char kPlayerApplicationId[] = "1557149187102220362";

int runWatchAndLaunch(const std::string& installDir, LaunchTarget target, const std::string& uri,
                       const std::string& currentVersion) {
    // The launcher and this watcher are the same binary, so without this they are the same name in
    // every process list, and anything matching the launcher matches a running Roblox session too.
    setProcessName("tuxbloxWatcher");

    Settings settings = loadSettings(installDir);

    // Runs before the active version is resolved below, so a freshly
    // installed version is the one that launches and gets its FastFlags.
    runRobloxUpdateCheck(installDir, target, settings);
    // Straight after the install rather than only once the session ends, so a
    // first-ever install gets its menu entry right away.
    exportPrefixShortcuts(installDir, selfExePath());
    // launchEnvPairs(), not parseEnvPairs(settings.envVars): a launch started
    // from a desktop shortcut has to carry the graphics-card selection too,
    // and this is the path that does not go through the running launcher.
    auto extraEnv = launchEnvPairs(settings);

    // Roblox reads FastFlags from a folder inside the version directory, and
    // every install produces a new one, so the file is rewritten here on each
    // launch rather than when the user edits a flag. Nothing is written on
    // the very first launch of a fresh prefix -- there is no version
    // directory until the official installer has produced one -- so flags
    // start applying from the launch after that.
    const std::string activeExe = resolveActiveVersionExePath(target, installDir);
    if (!activeExe.empty()) {
        const auto& flags = target == LaunchTarget::Player ? settings.fastFlags.player
                                                            : settings.fastFlags.studio;
        // A failure here is not worth refusing to start Roblox over: launching
        // without the overrides beats not launching at all.
        writeClientAppSettings(std::filesystem::path(activeExe).parent_path().string(), flags);
    }

    ProcessLauncher launcher(installDir);
    std::time_t launchStart = std::time(nullptr);
    auto outcome = launcher.launch(target, uri, extraEnv);
    if (!outcome.ok) {
        std::string message = "A TuxBlox process has exited with a non-zero exit code.\n" +
            outcome.errorMessage;
        showErrorMessageBox("TuxBlox Error", message);
        return 1;
    }

    // Presence rides the loop that was already here, so it costs no thread and no process of its own.
    // One switch covers both programs, and a game may change what is shown whenever presence is on.
    const bool presenceWanted = settings.discordRpc;
    const bool player = target == LaunchTarget::Player;
    std::unique_ptr<DiscordRpc> discord;
    std::unique_ptr<SessionLogTail> logTail;
    std::unique_ptr<PresenceReader> presenceReader;
    if (player) {
        presenceReader = std::make_unique<PlayerPresenceReader>();
    } else {
        presenceReader = std::make_unique<StudioPresenceReader>();
    }
    PresenceActivity lastSent;
    std::map<std::string, ResolvedPlace> resolvedPlaces;
    bool everSent = false;
    std::vector<std::time_t> recentSends;
    std::map<std::string, std::string> resolvedAssets;
    // The same icon the desktop shortcuts carry, so the notification looks like it came from Roblox rather than from the network.
    ServerRegionWatcher regionWatcher = makeServerRegionWatcher(
        settings.serverRegionNotify, player ? "tuxblox-roblox-player" : "tuxblox-roblox-studio");
    std::string lastServerAddress;
    if (presenceWanted) {
        discord = std::make_unique<DiscordRpc>(player ? kPlayerApplicationId : kStudioApplicationId);
    }

    while (launcher.pollIsRunning(target)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        // The log is read for either feature, so neither one being on is what stops the work.
        if (!presenceWanted && !settings.serverRegionNotify) {
            continue;
        }

        const std::time_t now = std::time(nullptr);
        if (!logTail) {
            const std::string logFile = findSessionLog(installDir, launchStart, player);
            if (!logFile.empty()) {
                logTail = std::make_unique<SessionLogTail>(logFile);
            }
        }
        if (logTail) {
            logTail->pump(*presenceReader);
        }

        // Each server is named once, and a reader whose target never reports one answers empty.
        const std::string serverAddress = presenceReader->serverAddress();
        if (!serverAddress.empty() && serverAddress != lastServerAddress) {
            lastServerAddress = serverAddress;
            regionWatcher.consumeServerAddress(serverAddress);
        }

        if (!presenceWanted) {
            continue;
        }

        discord->poll(now);
        PresenceActivity current = presenceReader->activity();
        // Dropped here rather than further down, so a game that asks for a status nobody wanted cannot make this fetch its art, or spend one of the few updates Discord allows on a presence that would read the same.
        if (!settings.discordGameStatus) {
            current.rpc = RpcOverride{};
        }

        // Studio names a published place only by number, so the name is asked for once per place and remembered, including when Roblox has none to give.
        // Roblox's own report is preferred over the join line, which a game could have written.
        const std::string describeId =
            current.reportedPlaceId.empty() ? current.placeId : current.reportedPlaceId;
        if (!describeId.empty()) {
            auto known = resolvedPlaces.find(describeId);
            if (known == resolvedPlaces.end()) {
                ResolvedPlace fetched;
                // A player knows the experience by its own name and its author, which is a different question from what the place inside it is called.
                UniverseInfo universe;
                const bool described = !current.universeId.empty()
                                           ? fetchUniverseInfoById(current.universeId, universe)
                                           : fetchUniverseInfo(describeId, universe);
                if (described) {
                    fetched.name = universe.name;
                    fetched.creatorLine = creatorLine(universe);
                } else if (current.placeName.empty()) {
                    fetched.name = fetchPlaceName(describeId);
                }
                // The experience's own icon, which belongs to its root place -- teleporting into a lobby inside it must not change the picture on somebody's profile.
                const std::string iconPlaceId =
                    described && !universe.rootPlaceId.empty() ? universe.rootPlaceId : describeId;
                fetched.iconUrl = fetchPlaceIconUrl(iconPlaceId);
                known = resolvedPlaces.emplace(describeId, fetched).first;
            }
            if (!known->second.name.empty()) {
                current.placeName = known->second.name;
            }
            current.placeIconUrl = known->second.iconUrl;
            current.creatorLine = known->second.creatorLine;
        }
        // Only a delivered activity counts: recording one Discord threw away would stop this ever trying again.
        resolveRpcImage(current.rpc.largeImage, resolvedAssets);
        resolveRpcImage(current.rpc.smallImage, resolvedAssets);

        if ((!everSent || current != lastSent) && rpcAllowsSend(recentSends, now) &&
            discord->send(activityJson(current, launchStart, settings.discordJoinButton), now)) {
            lastSent = current;
            everSent = true;
            recentSends.push_back(now);
        }
    }

    if (discord) {
        discord->clear(std::time(nullptr));
    }

    auto ev = launcher.takeExitEvent(target);

    // Everything that was copied is in our log now, so the file it came from is not left behind as a second log for the same session.
    const std::vector<std::string> appendedLogs =
        appendRobloxSessionLogs(installDir, launchStart, outcome.logPath);
    removeAppendedRobloxLogs(installDir, appendedLogs, player);
    exportPrefixShortcuts(installDir, selfExePath());

    if (outcome.wasBootstrapInstall && ev && !ev->stopRequested && ev->exitCode == 0) {
        // Only the first version of this program counts as installing it. An update brings another
        // one, and putting the icon back then would overrule somebody who had deleted it.
        const bool firstInstall = scanPrefixVersions(installDir, target).size() <= 1;
        registerBootstrappedVersion(installDir, target);
        if (firstInstall) {
            writeClientDesktopShortcut(installDir, selfExePath(), target);
        }
    }

    if (!ev || ev->stopRequested || ev->exitCode == 0) {
        return 0; // clean exit (or nothing to report) -- no UI at all
    }

    int protonExitCode = ev->exitCode;
    std::optional<int> robloxExitCode;
    if (ev->exitCode == 2) {
        robloxExitCode = findRealExitCodeInLog(outcome.logPath);
    }
    int displayCode = robloxExitCode.value_or(protonExitCode);

    const char* title = exitCodeTitle(displayCode);
    std::string exitCodeLine = "Exit Code: " + std::to_string(displayCode);
    if (title) exitCodeLine += std::string(" (") + title + ")";
    exitCodeLine += "\n";

    std::string popupTitle, message;
    if (ev->exitCode == 3) {
        // Integrity and certificate verification, TuxBlox's compatibility layer always returns exit code 3 if this failed.
        showErrorMessageBox("Failed to launch Roblox",
            "Failed to verify the integrity of Roblox. Some files might be unsigned or have "
            "been tampered with. Please make sure TuxBlox is up to date, then reinstall "
            "Roblox and try again.");
        return 1;
    }

    if (ev->exitCode == 1) {
        popupTitle = "TuxBlox Error";
        message = "TuxBlox has encountered an error and has quit!\n"
            "Full log has been written to " + outcome.logPath;
    } else if (ev->exitCode == 2) {
        popupTitle = "Roblox Error";
        message = "Roblox has exited with a non-zero exit code.\n" +
            exitCodeLine + "Full log has been written to " + outcome.logPath;
    } else {
        popupTitle = "Something went wrong";
        message = "An unknown error has occurred which has crashed TuxBlox.\n" +
            exitCodeLine + "Full log has been written to " + outcome.logPath;
    }

    auto sendReport = [&] {
        CrashReport report;
        report.launcherVersion = currentVersion;
        report.protonVersion = readInstalledCompatVersion(installDir).value_or("");
        report.target = target;
        report.protonExitCode = protonExitCode;
        report.robloxExitCode = robloxExitCode;
        report.logPath = outcome.logPath;
        report.systemInfo = collectSystemInfo();
        uploadCrashReport(report);
    };

    if (settings.sendCrashReports) {
        sendReport();
        showErrorMessageBox(popupTitle, message);
    } else if (showErrorMessageBoxWithAction(popupTitle, message, "Report")) {
        sendReport();
    }

    return 1;
}

} // namespace tuxblox
