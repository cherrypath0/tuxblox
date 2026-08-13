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
#include "lnk_resolver.h"
#include "settings.h"
#include "updater.h"

namespace tuxblox {

enum class Tab { Start, About, Settings };

struct UninstallProgress {
    // True from the moment requestUninstall() is called until either the
    // uninstaller handoff fires (App::needsUninstallHandoff()) or it fails
    // (errorMessage gets set) -- drives the Settings tab's button showing a
    // disabled "Uninstalling..." state instead of the two-click confirm.
    bool inProgress = false;
    std::string errorMessage; // non-empty iff the last attempt failed
};

// Same shape as UninstallProgress, for the Wipe Prefix button.
struct WipePrefixProgress {
    bool inProgress = false;
    std::string errorMessage;
};

struct AppSnapshot {
    UpdateProgress update;
    UninstallProgress uninstall;
    WipePrefixProgress wipePrefix;
    Tab activeTab = Tab::Start;
    Settings settings;
    // Set once at startup (App's constructor) if running inside a
    // Distrobox container without an apparent GPU device node -- empty if
    // there's nothing to warn about. Never cleared back to empty by the
    // UI -- only whether it has already been *shown* is tracked, in Ui's
    // own state (see ui.h).
    std::string containerWarning;
};

class App {
public:
    App(std::string installDir, std::string currentVersion, std::string launcherExePath);
    ~App();

    void startUpdateCheck();

    // True once the update check has determined a verified installer
    // binary is ready to be exec'd -- the caller should stop the render
    // loop, exec() installerHandoffPath(), and never return.
    bool needsInstallerHandoff() const;
    std::string installerHandoffPath() const;

    void setActiveTab(Tab tab);

    // Spawns a fully detached "--watch-launch" process (see watch_launch.h)
    // that starts `target` via Proton and, from that point on, is the only
    // thing tracking it: it waits for the process, shows a crash popup if
    // warranted, and otherwise produces no UI at all. This process has no
    // further involvement -- no tray icon, no hidden window kept around --
    // it just closes (shouldQuit() below drives that). Tried backgrounding
    // to a system tray icon first; abandoned after extensive testing showed
    // xembedsniproxy (KDE's XEmbed-to-StatusNotifierItem proxy) takes over
    // the icon window once docked and rejects any further property/attribute
    // change against it under this desktop's specific KWin/XWayland setup,
    // regardless of which standard mechanism was tried (override_redirect,
    // EWMH dock/skip-taskbar hints, before or after the dock handshake).
    void requestLaunch(LaunchTarget target);
    // True once requestLaunch() has successfully spawned the detached
    // watcher process -- Ui's render loop checks this to end immediately.
    bool shouldQuit() const;

    // Fetches the latest release's manifest for the current channel,
    // ensures a verified TuxBloxInstaller binary is staged at
    // <installDir>/TuxBloxInstaller (same fetch-verify dance as an update
    // handoff -- see updater.h's ensureInstallerBinary), and once ready
    // sets needsUninstallHandoff(). Runs in the background; snapshot().uninstall
    // reflects progress/errors for the UI. A no-op while already in
    // progress (repeat clicks on a disabled button shouldn't restart it).
    void requestUninstall();
    // True once requestUninstall() has a verified installer ready to be
    // exec'd with --uninstall -- the caller should stop the render loop,
    // exec() installerHandoffPath() with that flag, and never return.
    bool needsUninstallHandoff() const;

    // Deletes the contents of <installDir>/runtime (the Wine prefix) in the
    // background, leaving the runtime/ directory itself in place empty --
    // the next launch recreates whatever it needs inside it. Purely local
    // (no network, unlike requestUninstall()), but still backgrounded since
    // a heavily-used prefix can be a few GB and this shouldn't freeze the
    // UI thread. snapshot().wipePrefix reflects progress/errors. A no-op
    // while already in progress.
    void requestWipePrefix();

    AppSnapshot snapshot() const;

    // Persists `settings` (settings.json), re-applies Global Environment
    // Variables to the launcher's own process, and updates the snapshot.
    void updateSettings(Settings settings);

private:
    void updateCheckThreadMain();
    void uninstallThreadMain();
    void wipePrefixThreadMain();
    void applyGlobalEnvVars(const std::string& globalEnvVars);

    std::string installDir_;
    std::string currentVersion_;
    std::string launcherExePath_;

    mutable std::mutex mutex_;
    AppSnapshot snapshot_;
    std::atomic<bool> needsInstallerHandoff_{false};
    std::atomic<bool> needsUninstallHandoff_{false};
    std::atomic<bool> shouldQuit_{false};
    // Written once by whichever of updateCheckThreadMain/uninstallThreadMain
    // gets there first, strictly before that same thread sets its handoff
    // flag -- same happens-before idiom as needsInstallerHandoff_ itself.
    // The two threads' write windows don't overlap in practice (uninstall
    // is a rare, explicit user action; update-check runs once at startup),
    // so a single shared field is fine without extra synchronization.
    std::string installerHandoffPath_;
    std::atomic<bool> updateCancel_{false};
    std::atomic<bool> uninstallCancel_{false};

    std::thread updateThread_;
    std::thread uninstallThread_;
    // No cancel flag: unlike the two threads above (both network-bound,
    // hence cancellable so shutdown isn't held hostage by a slow download),
    // this is a local fs::remove_all with no cancellation point of its own
    // -- the destructor just joins it and accepts the (typically brief)
    // wait.
    std::thread wipePrefixThread_;
};

} // namespace tuxblox
