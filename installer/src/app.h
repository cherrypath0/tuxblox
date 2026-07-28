#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "progress.h"

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
    Step currentStep = Step::CreatingDirectory;
    double overallPercent = 0.0;
    std::string errorMessage;
    // True if an existing install was found and this run is upgrading it in
    // place rather than doing a fresh install -- selects "Upgrading ..."
    // step wording (see stepLabel(Step, bool)) and changes cleanup-on-failure
    // behavior (an upgrade failure must never wipe the user's existing
    // install, only whatever partial files this run itself created).
    bool isUpgrade = false;
};

// Owns the background install thread and exposes a thread-safe snapshot
// of current state for the UI to poll each frame.
class App {
public:
    App();
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

    mutable std::mutex mutex_;
    AppSnapshot snapshot_;
    std::atomic<bool> cancelRequested_{false};
    std::atomic<bool> readyToLaunch_{false};
    std::string launcherPath_;
    std::thread thread_;
};

} // namespace tuxblox
