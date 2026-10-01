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
#include "roblox_autoupdate.h"
#include "roblox_log_capture.h"
#include "roblox_place_info.h"
#include "settings.h"
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
#include <memory>
#include <thread>

namespace tuxblox {

// What Roblox told us about a place, asked for once and kept for the session.
struct ResolvedPlace {
    std::string name;
    std::string iconUrl;
};

// The Discord application this presence is published under, registered at discord.com/developers. Public, not a secret. Empty would mean nothing is ever sent.
const char kDiscordApplicationId[] = "1554221498925973584";

int runWatchAndLaunch(const std::string& installDir, LaunchTarget target, const std::string& uri,
                       const std::string& currentVersion) {
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
    auto outcome = launcher.launch(target, uri, extraEnv, settings.verifyIntegrity);
    if (!outcome.ok) {
        std::string message = "A TuxBlox process has exited with a non-zero exit code.\n" +
            outcome.errorMessage;
        showErrorMessageBox("TuxBlox Error", message);
        return 1;
    }

    // Presence rides the loop that was already here, so it costs no thread and no process of its own.
    const bool presenceWanted = settings.discordRpc && target == LaunchTarget::Studio;
    std::unique_ptr<DiscordRpc> discord;
    std::unique_ptr<SessionLogTail> logTail;
    StudioPresenceReader presenceReader;
    PresenceActivity lastSent;
    std::map<std::string, ResolvedPlace> resolvedPlaces;
    bool everSent = false;
    if (presenceWanted) {
        discord = std::make_unique<DiscordRpc>(kDiscordApplicationId);
    }

    while (launcher.pollIsRunning(target)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (!presenceWanted) {
            continue;
        }

        const std::time_t now = std::time(nullptr);
        if (!logTail) {
            const std::string logFile = findStudioSessionLog(installDir, launchStart);
            if (!logFile.empty()) {
                logTail = std::make_unique<SessionLogTail>(logFile);
            }
        }
        if (logTail) {
            logTail->pump(presenceReader);
        }

        discord->poll(now);
        PresenceActivity current = presenceReader.activity();

        // Studio names a published place only by number, so the name is asked for once per place and remembered, including when Roblox has none to give.
        if (!current.placeId.empty()) {
            auto known = resolvedPlaces.find(current.placeId);
            if (known == resolvedPlaces.end()) {
                ResolvedPlace fetched;
                if (current.placeName.empty()) {
                    fetched.name = fetchPlaceName(current.placeId);
                }
                fetched.iconUrl = fetchPlaceIconUrl(current.placeId);
                known = resolvedPlaces.emplace(current.placeId, fetched).first;
            }
            if (current.placeName.empty()) {
                current.placeName = known->second.name;
            }
            current.placeIconUrl = known->second.iconUrl;
        }
        // Only a delivered activity counts: recording one Discord threw away would stop this ever trying again.
        if ((!everSent || current != lastSent) &&
            discord->send(activityJson(current, launchStart), now)) {
            lastSent = current;
            everSent = true;
        }
    }

    if (discord) {
        discord->clear(std::time(nullptr));
    }

    auto ev = launcher.takeExitEvent(target);

    appendRobloxSessionLogs(installDir, launchStart, outcome.logPath);
    exportPrefixShortcuts(installDir, selfExePath());

    if (outcome.wasBootstrapInstall && ev && !ev->stopRequested && ev->exitCode == 0) {
        registerBootstrappedVersion(installDir, target);
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
