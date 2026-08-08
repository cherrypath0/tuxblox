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
#include "container_env.h"
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

namespace {
constexpr const char* kManifestUrl = "https://assetdelivery.tuxblox.net/pkg/manifest.json";
} // namespace

App::App(std::string installDir, std::string currentVersion, std::string launcherExePath)
    : installDir_(std::move(installDir)),
      currentVersion_(std::move(currentVersion)),
      launcherExePath_(std::move(launcherExePath)) {
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
}

void App::startUpdateCheck() {
    if (updateThread_.joinable()) return; // already started -- safe to call once
    updateThread_ = std::thread(&App::updateCheckThreadMain, this);
}

bool App::needsInstallerHandoff() const {
    return needsInstallerHandoff_.load();
}

bool App::shouldQuit() const {
    return shouldQuit_.load();
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
    if (shouldQuit_.load()) return; // already spawned one -- ignore further clicks

    pid_t pid = fork();
    if (pid < 0) return; // fork failed -- nothing else to do, stay open
    if (pid == 0) {
        // Double-fork detach, same pattern as ui.cpp's openUrl(): the
        // immediate child exits right away, the grandchild (the actual
        // --watch-launch process) is re-parented to init so it outlives
        // this whole launcher cleanly, with no zombie left behind.
        pid_t inner = fork();
        if (inner == 0) {
            setsid();
            const char* targetArg = (target == LaunchTarget::Player) ? "player" : "studio";
            execl(launcherExePath_.c_str(), launcherExePath_.c_str(), "--watch-launch", targetArg,
                  static_cast<char*>(nullptr));
            _exit(127); // only reached if execl itself failed
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0); // reap the immediate child; it exits almost instantly

    shouldQuit_.store(true);
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

} // namespace tuxblox
