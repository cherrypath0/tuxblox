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

#include "app.h"
#include "crash_report.h"
#include "install_paths.h"
#include "container_env.h"
#include <cstdlib>
#include <filesystem>

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
    snapshot_.settings = loadSettings(installDir_);
    // Safe to call unlocked here: the constructor runs before
    // startUpdateCheck() spawns any other thread, so nothing else can be
    // concurrently calling getenv() yet. See applyGlobalEnvVars()'s own
    // comment for why that ordering matters everywhere else it's called.
    applyGlobalEnvVars(snapshot_.settings.globalEnvVars);

    // A missing /dev/dri inside a Distrobox container almost always means
    // the container was created without GPU passthrough -- Roblox will
    // fail to render under Proton/DXVK. Check-and-warn only: a missing
    // device node can't be fixed from inside the container, so this exists
    // purely to turn a confusing downstream crash into an actionable
    // message before the user even tries to launch.
    if (isInsideDistrobox() && !std::filesystem::exists("/dev/dri")) {
        snapshot_.containerWarning =
            "Running inside a Distrobox container without GPU passthrough -- "
            "Roblox will likely fail to render. Recreate the container with "
            "GPU access, e.g. `distrobox create --nvidia ...` or "
            "`--additional-flags \"--device /dev/dri\"`.";
    }
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
    auto playerExit = processLauncher_.takeExitEvent(LaunchTarget::Player);
    auto studioExit = processLauncher_.takeExitEvent(LaunchTarget::Studio);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.playerRunning = playerRunning;
        snapshot_.studioRunning = studioRunning;
        snapshot_.playerActionInFlight = playerActionInFlight_.load();
        snapshot_.studioActionInFlight = studioActionInFlight_.load();
    }
    if (playerExit && playerExit->exitCode != 0 && !playerExit->stopRequested) {
        handleUnexpectedExit(LaunchTarget::Player, playerExit->exitCode);
    }
    if (studioExit && studioExit->exitCode != 0 && !studioExit->stopRequested) {
        handleUnexpectedExit(LaunchTarget::Studio, studioExit->exitCode);
    }
}

AppSnapshot App::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void App::updateSettings(Settings settings) {
    saveSettings(installDir_, settings);
    bool globalEnvChanged;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        globalEnvChanged = snapshot_.settings.globalEnvVars != settings.globalEnvVars;
        snapshot_.settings = settings;
    }
    if (globalEnvChanged) {
        applyGlobalEnvVars(settings.globalEnvVars);
    }
}

void App::clearCrashNotice() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.crashNotice = CrashNotice{};
}

void App::applyGlobalEnvVars(const std::string& globalEnvVars) {
    // setenv()/getenv() are not thread-safe in glibc (Finding 6,
    // 2026-07-28 final review) -- this process does have other threads
    // that may call getenv() (the update-check thread, inside curl).
    // Unlike the Proton-child env vars, Global Environment Variables are
    // deliberately applied to the launcher's own process (real "export"
    // semantics -- see the settings design doc), so that hazard can't be
    // avoided by scoping to a forked child the way launchEnvVars() is. The
    // constructor call site is race-free (nothing else is running yet);
    // later calls from updateSettings() (user edits, on the render thread)
    // accept the same small, already-documented race rather than adding
    // cross-thread coordination for a rare, user-initiated edit.
    for (const auto& kv : parseEnvPairs(globalEnvVars)) {
        auto pos = kv.find('=');
        setenv(kv.substr(0, pos).c_str(), kv.substr(pos + 1).c_str(), 1);
    }
}

void App::handleUnexpectedExit(LaunchTarget target, int exitCode) {
    Settings settings;
    std::string logPath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        logPath = (target == LaunchTarget::Player) ? playerLogPath_ : studioLogPath_;
        settings = snapshot_.settings;

        CrashNotice notice;
        notice.pending = true;
        notice.title = "Roblox Error";
        notice.message = std::string("Roblox has exited with a non-zero exit code.\n") +
            "Exit Code: " + std::to_string(exitCode) + " (" + exitCodeTitle(exitCode) + ")\n" +
            "Full log has been written to " + logPath;
        snapshot_.crashNotice = notice;
    }

    if (settings.sendCrashReports) {
        CrashReport report;
        report.launcherVersion = currentVersion_;
        report.protonVersion = readInstalledProtonVersion(installDir_).value_or("");
        report.target = target;
        report.exitCode = exitCode;
        report.logPath = logPath;
        // Detached, not joined: this must never block the render thread,
        // and uploadCrashReport() only touches its own by-value copy of
        // `report`, never App state, so it's safe to outlive this call
        // (and even outlive App itself, bounded by its own short timeout).
        std::thread(uploadCrashReport, report).detach();
    }
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
    std::vector<std::string> extraEnv;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        extraEnv = parseEnvPairs(snapshot_.settings.protonEnvVars);
    }
    auto outcome = processLauncher_.launch(target, "", extraEnv);
    std::lock_guard<std::mutex> lock(mutex_);
    std::string& errSlot = (target == LaunchTarget::Player) ? snapshot_.playerError : snapshot_.studioError;
    errSlot = outcome.ok ? "" : outcome.errorMessage;
    if (outcome.ok) {
        (target == LaunchTarget::Player ? playerLogPath_ : studioLogPath_) = outcome.logPath;
    }
    (target == LaunchTarget::Player ? playerActionInFlight_ : studioActionInFlight_).store(false);
}

void App::stopThreadMain(LaunchTarget target) {
    processLauncher_.stop(target);
    std::lock_guard<std::mutex> lock(mutex_);
    (target == LaunchTarget::Player ? playerActionInFlight_ : studioActionInFlight_).store(false);
}

} // namespace tuxblox
