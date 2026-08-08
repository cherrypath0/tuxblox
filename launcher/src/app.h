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

struct AppSnapshot {
    UpdateProgress update;
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

    AppSnapshot snapshot() const;

    // Persists `settings` (settings.json), re-applies Global Environment
    // Variables to the launcher's own process, and updates the snapshot.
    void updateSettings(Settings settings);

private:
    void updateCheckThreadMain();
    void applyGlobalEnvVars(const std::string& globalEnvVars);

    std::string installDir_;
    std::string currentVersion_;
    std::string launcherExePath_;

    mutable std::mutex mutex_;
    AppSnapshot snapshot_;
    std::atomic<bool> needsInstallerHandoff_{false};
    std::atomic<bool> shouldQuit_{false};
    std::string installerHandoffPath_; // written once, before needsInstallerHandoff_ is set -- see updateCheckThreadMain
    std::atomic<bool> updateCancel_{false};

    std::thread updateThread_;
};

} // namespace tuxblox
