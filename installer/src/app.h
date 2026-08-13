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

namespace tuxblox {

enum class AppPhase {
    Init,
    FetchingManifest,
    Installing,
    Error,
    Done
};

struct AppSnapshot {
    AppPhase phase = AppPhase::Init;
    // Already fully resolved (fresh-install vs "Upgrading ..." wording
    // decided at the point it's generated -- see installer_steps.cpp) since
    // the label set isn't fixed at compile time, it's built per-artifact
    // from whatever the manifest lists.
    std::string currentStepLabel = "Preparing";
    double overallPercent = 0.0;
    std::string errorMessage;
    // True if an existing install was found and this run is upgrading it in
    // place rather than doing a fresh install -- changes cleanup-on-failure
    // behavior (an upgrade failure must never wipe the user's existing
    // install, only whatever partial files this run itself created).
    bool isUpgrade = false;
};

// Owns the background install thread and exposes a thread-safe snapshot
// of current state for the UI to poll each frame.
class App {
public:
    // `channel` selects which /v1/<channel>/... release to install/upgrade
    // to -- "stable" when run standalone (main.cpp's default), or whatever
    // the launcher passed via --channel when handing off an upgrade.
    explicit App(std::string channel = "stable");
    ~App();

    // Starts the background install pipeline (INIT -> ... -> DONE/ERROR).
    // Safe to call once.
    void start();

    // Signals cancellation; the background thread will stop at its next
    // checkpoint and clean up partial state.
    void cancel();

    // Thread-safe read of current state for rendering.
    AppSnapshot snapshot() const;

    // True once the background thread has reached Done and the launcher
    // is ready to be exec'd by the caller (main.cpp performs the actual
    // exec() call on the main thread after the render loop exits).
    bool readyToLaunch() const;

    std::string launcherPath() const;

private:
    void run(); // background thread entry point

    std::string channel_;
    mutable std::mutex mutex_;
    AppSnapshot snapshot_;
    std::atomic<bool> cancelRequested_{false};
    std::atomic<bool> readyToLaunch_{false};
    std::string launcherPath_;
    std::thread thread_;
};

} // namespace tuxblox
