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
#include "cli.h"
#include "config.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace tuxblox {

enum class Phase { Idle, Working, Error, Done };

struct Snapshot {
    Phase phase = Phase::Idle;
    // The one line of text in the window. Starts as the mode -- "Preview
    // mode", "Checking for updates..." -- then becomes the step in progress,
    // e.g. "Downloading RobloxApp.zip".
    std::string status = "Starting";
    double overallPercent = 0.0;
    std::string errorMessage;
};

// Owns the background thread that does the work and exposes a thread-safe
// snapshot for the window to poll each frame.
class App {
public:
    App(Mode mode, Config config);
    ~App();

    // Starts the background thread. Safe to call once.
    void start();

    // Signals cancellation; the thread stops at its next checkpoint.
    void cancel();

    Snapshot snapshot() const;

    // True once the thread has reached Done or Error.
    bool finished() const;

    // Preview leaves the window up so it can be looked at; the working modes
    // close on their own, because the launcher waits for this process.
    bool holdsOpenWhenDone() const;

    // Whether a window should be on screen yet. True from the start for the
    // modes the user asked for directly, but an update check runs before
    // every launch and is usually a few hundred milliseconds ending in
    // "already up to date" -- so it stays headless until it knows it has a
    // download to do, and flips this only then.
    bool needsWindow() const { return needsWindow_; }

private:
    void run();
    void runPreview();
    void runInstall(bool skipIfPresent);
    void setStatus(const std::string& status, double percent);
    void finish(const std::string& message);
    void fail(const std::string& message);

    Mode mode_;
    Config config_;
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> needsWindow_{false};
    std::thread thread_;
};

} // namespace tuxblox
