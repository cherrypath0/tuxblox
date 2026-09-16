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
#include "install_paths.h"
#include "process_launcher.h"
#include "roblox_log_capture.h"
#include "settings.h"
#include "system_info.h"
#include "ui_qt/message_box.h"
#include "fastflag_file.h"
#include "versions_manifest.h"
#include "wine_shortcut_export.h"
#include <chrono>
#include <filesystem>
#include <ctime>
#include <thread>

namespace tuxblox {

int runWatchAndLaunch(const std::string& installDir, LaunchTarget target, const std::string& uri,
                       const std::string& currentVersion) {
    Settings settings = loadSettings(installDir);
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

    while (launcher.pollIsRunning(target)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    auto ev = launcher.takeExitEvent(target);

    // Item 10 (plan/todo.md): fold Roblox's own session log(s) into this
    // launch's log file, unconditionally -- a clean exit still gets its
    // Roblox log recorded, this isn't gated on crash detection below.
    appendRobloxSessionLogs(installDir, launchStart, outcome.logPath);

    // A fresh install has no c:\proton_shortcuts entries until Roblox's own
    // installer has run, which happens during this very session -- so refresh
    // the exported app-menu entries here rather than making the user wait for
    // the next GUI start. Best-effort, same as everything else in this area.
    exportPrefixShortcuts(installDir, selfExePath());

    if (outcome.wasBootstrapInstall && ev && !ev->stopRequested && ev->exitCode == 0) {
        // The official RobloxPlayerInstaller.exe/RobloxStudioInstaller.exe
        // just ran (via resolveOrBootstrapExePath's existing fallback) and
        // exited cleanly -- record whatever version it installed so future
        // launches use the pinned-version path (Task 5) instead of
        // re-invoking the installer every time.
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

    if (settings.sendCrashReports) {
        CrashReport report;
        report.launcherVersion = currentVersion;
        report.protonVersion = readInstalledCompatVersion(installDir).value_or("");
        report.target = target;
        report.protonExitCode = protonExitCode;
        report.robloxExitCode = robloxExitCode;
        report.logPath = outcome.logPath;
        report.systemInfo = collectSystemInfo();

        uploadCrashReport(report);
    }

    showErrorMessageBox(popupTitle, message);

    return 1;
}

} // namespace tuxblox
