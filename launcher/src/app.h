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
    // Set once at startup (App's constructor) if running inside a
    // Distrobox container without an apparent GPU device node -- empty if
    // there's nothing to warn about. Unlike CrashNotice, this describes a
    // standing fact about the environment rather than a one-off event, so
    // it's never cleared back to empty by the UI -- only whether it has
    // already been *shown* is tracked, in Ui's own state (see ui.h).
    std::string containerWarning;
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

    // True once a launch (Player or Studio) has actually started -- Ui uses
    // this to hide the main window and show a tray icon instead, per
    // plan/plan.txt item 24 and item 1's "goes to the background until it
    // exits" behavior. Stays true for the rest of this process's lifetime
    // once set (there is currently no path back to the normal foreground
    // GUI -- the backgrounded run always ends in shouldQuit()).
    bool isBackgrounded() const;
    // True once the backgrounded launch's outcome has been decided and the
    // whole application should exit: either the process exited cleanly (0),
    // the user explicitly stopped it, or it crashed and the resulting
    // crashNotice has already been queued for display. The render loop
    // checks this after showing any pending crashNotice, so the popup (if
    // any) is still seen before the app actually closes.
    bool shouldQuit() const;

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
    // Called from pollProcesses() for every running->stopped transition
    // while backgrounded (see isBackgrounded()) -- decides whether this is
    // a quiet exit (clean 0, or a user-initiated stop) or a crash (routes to
    // handleUnexpectedExit), and either way sets quitRequested_ so the whole
    // launcher closes once the outcome (and any popup) has been shown.
    void handleTrackedExit(LaunchTarget target, const ExitEvent& ev);

    std::string installDir_;
    std::string currentVersion_;

    mutable std::mutex mutex_;
    AppSnapshot snapshot_;
    std::atomic<bool> needsInstallerHandoff_{false};
    std::atomic<bool> backgrounded_{false};
    std::atomic<bool> quitRequested_{false};
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
