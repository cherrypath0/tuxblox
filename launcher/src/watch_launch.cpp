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
#include "versions_manifest.h"
#include <chrono>
#include <ctime>
#include <thread>

namespace tuxblox {

int runWatchAndLaunch(const std::string& installDir, LaunchTarget target, const std::string& uri,
                       const std::string& currentVersion) {
    Settings settings = loadSettings(installDir);
    auto extraEnv = parseEnvPairs(settings.protonEnvVars);

    ProcessLauncher launcher(installDir);
    std::time_t launchStart = std::time(nullptr);
    auto outcome = launcher.launch(target, uri, extraEnv);
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
    if (ev->exitCode == 1) {
        // Proton itself failed before/while supervising the process -- not
        // Roblox's fault. See plan/plan.txt item 1's "if not roblox" template.
        popupTitle = "TuxBlox Error";
        message = "A TuxBlox process has exited with a non-zero exit code.\n" +
            exitCodeLine + "Full log has been written to " + outcome.logPath;
    } else {
        popupTitle = "Roblox Error";
        message = "Roblox has exited with a non-zero exit code.\n" +
            exitCodeLine + "Full log has been written to " + outcome.logPath;
    }

    if (settings.sendCrashReports) {
        CrashReport report;
        report.launcherVersion = currentVersion;
        report.protonVersion = readInstalledProtonVersion(installDir).value_or("");
        report.target = target;
        report.protonExitCode = protonExitCode;
        report.robloxExitCode = robloxExitCode;
        report.logPath = outcome.logPath;
        report.systemInfo = collectSystemInfo();
        // This whole process exits right after showing the popup below, so
        // there's no point detaching this the way the old in-GUI version
        // did (nothing else is running here for it to avoid blocking) --
        // just let it complete before we exit, bounded by its own internal
        // 5s/10s timeouts.
        uploadCrashReport(report);
    }

    showErrorMessageBox(popupTitle, message);

    return 1;
}

} // namespace tuxblox
