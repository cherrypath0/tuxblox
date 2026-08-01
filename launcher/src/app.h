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

#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "process_launcher.h"
#include "settings.h"
#include "updater.h"

namespace tuxblox {

enum class Tab { Start, About, Settings };

// A one-shot notice for the UI to show via SDL_ShowSimpleMessageBox, then
// clear -- populated when a tracked process (Player/Studio) exits with a
// non-zero code that wasn't the result of the user hitting Stop. See
// plan/plan.txt item 1.
struct CrashNotice {
    bool pending = false;
    std::string title;
    std::string message;
};

struct AppSnapshot {
    UpdateProgress update;
    bool playerRunning = false;
    bool studioRunning = false;
    bool playerActionInFlight = false;
    bool studioActionInFlight = false;
    std::string playerError;
    std::string studioError;
    Tab activeTab = Tab::Start;
    Settings settings;
    CrashNotice crashNotice;
};

class App {
public:
    App(std::string installDir, std::string currentVersion);
    ~App();

    void startUpdateCheck();

    // True once the update check has determined a verified installer
    // binary is ready to be exec'd -- the caller should stop the render
    // loop, exec() installerHandoffPath(), and never return.
    bool needsInstallerHandoff() const;
    std::string installerHandoffPath() const;

    void setActiveTab(Tab tab);
    void requestLaunch(LaunchTarget target);
    void requestStop(LaunchTarget target);
    void pollProcesses();
    AppSnapshot snapshot() const;

    // Persists `settings` (settings.json), re-applies Global Environment
    // Variables to the launcher's own process, and updates the snapshot.
    void updateSettings(Settings settings);
    // Clears snapshot().crashNotice.pending after the UI has shown it.
    void clearCrashNotice();

private:
    void updateCheckThreadMain();
    void launchThreadMain(LaunchTarget target);
    void stopThreadMain(LaunchTarget target);
    void applyGlobalEnvVars(const std::string& globalEnvVars);
    // Called from pollProcesses() on a running->stopped transition that
    // wasn't a user-requested stop: populates the crash notice and, if
    // enabled, fires off a telemetry upload on its own detached thread.
    void handleUnexpectedExit(LaunchTarget target, int exitCode);

    std::string installDir_;
    std::string currentVersion_;

    mutable std::mutex mutex_;
    AppSnapshot snapshot_;
    std::atomic<bool> needsInstallerHandoff_{false};
    std::string installerHandoffPath_; // written once, before needsInstallerHandoff_ is set -- see updateCheckThreadMain
    std::atomic<bool> updateCancel_{false};
    std::atomic<bool> playerActionInFlight_{false};
    std::atomic<bool> studioActionInFlight_{false};

    // Log path written by the most recent launch of each target -- captured
    // from LaunchOutcome so a later crash notice/telemetry upload (which
    // only gets an exit code from ProcessLauncher::takeExitEvent) knows
    // which file to point at. Guarded by mutex_, same as snapshot_.
    std::string playerLogPath_;
    std::string studioLogPath_;

    ProcessLauncher processLauncher_;
    std::thread updateThread_;
    std::thread playerActionThread_;
    std::thread studioActionThread_;
};

} // namespace tuxblox
