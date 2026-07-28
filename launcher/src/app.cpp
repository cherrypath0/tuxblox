#include "app.h"

namespace tuxblox {

namespace {
constexpr const char* kManifestUrl = "https://assetdelivery.tuxblox.net/pkg/manifest.json";
} // namespace

App::App(std::string installDir, std::string currentVersion)
    : installDir_(std::move(installDir)),
      currentVersion_(std::move(currentVersion)),
      processLauncher_(installDir_) {
    // Desktop integration (ensureDesktopIntegration) is deliberately NOT
    // called here -- it's a bounded-but-blocking call (up to ~12s worst
    // case) and this constructor runs before the window is shown. It's
    // invoked explicitly from main.cpp instead, after Ui::init(), so the
    // window exists before that potential stall. See Finding 5, 2026-07-28
    // final review.
}

App::~App() {
    // Let an in-flight update download (the installer binary, or -- before
    // this handed off to it -- Proton) abort promptly instead of blocking
    // this join for however long the download would otherwise take -- the
    // window is already gone by the time the destructor runs. See Finding
    // 2, 2026-07-28 final review.
    updateCancel_.store(true);
    if (updateThread_.joinable()) updateThread_.join();
    if (playerActionThread_.joinable()) playerActionThread_.join();
    if (studioActionThread_.joinable()) studioActionThread_.join();
}

void App::startUpdateCheck() {
    if (updateThread_.joinable()) return; // already started -- safe to call once
    updateThread_ = std::thread(&App::updateCheckThreadMain, this);
}

bool App::needsInstallerHandoff() const {
    return needsInstallerHandoff_.load();
}

std::string App::installerHandoffPath() const {
    // Safe without a lock: installerHandoffPath_ is written in
    // updateCheckThreadMain() strictly before the release-store to
    // needsInstallerHandoff_ below, and callers only read this after
    // needsInstallerHandoff() has returned true (an acquire-load of the
    // same atomic) -- the standard "flag variable" happens-before idiom.
    return installerHandoffPath_;
}

void App::setActiveTab(Tab tab) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.activeTab = tab;
}

void App::requestLaunch(LaunchTarget target) {
    std::atomic<bool>& inFlight = (target == LaunchTarget::Player) ? playerActionInFlight_ : studioActionInFlight_;
    // If an action for this target is already running, no-op rather than
    // joining here on the render thread -- the previous action (e.g. a
    // Stop's ~2s SIGTERM grace period, or an installer download) might
    // still be in progress and join() would block the whole window.
    if (inFlight.exchange(true)) return;
    std::thread& slot = (target == LaunchTarget::Player) ? playerActionThread_ : studioActionThread_;
    // inFlight was false, so the previous thread (if any) has already
    // finished its work and cleared the flag -- join() here just reaps an
    // already-exited thread, it does not block.
    if (slot.joinable()) slot.join();
    slot = std::thread(&App::launchThreadMain, this, target);
}

void App::requestStop(LaunchTarget target) {
    std::atomic<bool>& inFlight = (target == LaunchTarget::Player) ? playerActionInFlight_ : studioActionInFlight_;
    if (inFlight.exchange(true)) return;
    std::thread& slot = (target == LaunchTarget::Player) ? playerActionThread_ : studioActionThread_;
    if (slot.joinable()) slot.join();
    slot = std::thread(&App::stopThreadMain, this, target);
}

void App::pollProcesses() {
    bool playerRunning = processLauncher_.pollIsRunning(LaunchTarget::Player);
    bool studioRunning = processLauncher_.pollIsRunning(LaunchTarget::Studio);
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.playerRunning = playerRunning;
    snapshot_.studioRunning = studioRunning;
    snapshot_.playerActionInFlight = playerActionInFlight_.load();
    snapshot_.studioActionInFlight = studioActionInFlight_.load();
}

AppSnapshot App::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void App::updateCheckThreadMain() {
    auto result = runUpdateCheck(currentVersion_, kManifestUrl,
        [&](UpdateProgress p) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.update = p;
        },
        &updateCancel_, installDir_);
    if (result.needsHandoff) {
        installerHandoffPath_ = result.installerPath; // see installerHandoffPath()'s comment
        needsInstallerHandoff_.store(true);
    }
}

void App::launchThreadMain(LaunchTarget target) {
    auto outcome = processLauncher_.launch(target);
    std::lock_guard<std::mutex> lock(mutex_);
    std::string& errSlot = (target == LaunchTarget::Player) ? snapshot_.playerError : snapshot_.studioError;
    errSlot = outcome.ok ? "" : outcome.errorMessage;
    (target == LaunchTarget::Player ? playerActionInFlight_ : studioActionInFlight_).store(false);
}

void App::stopThreadMain(LaunchTarget target) {
    processLauncher_.stop(target);
    std::lock_guard<std::mutex> lock(mutex_);
    (target == LaunchTarget::Player ? playerActionInFlight_ : studioActionInFlight_).store(false);
}

} // namespace tuxblox
