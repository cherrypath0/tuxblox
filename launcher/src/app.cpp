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
#include "manifest.h"
#include <filesystem>
#include <optional>
#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

namespace {
constexpr const char* kSetupBaseUrl = "https://setup.tuxblox.net";
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
    uninstallCancel_.store(true);
    if (updateThread_.joinable()) updateThread_.join();
    if (uninstallThread_.joinable()) uninstallThread_.join();
    if (wipePrefixThread_.joinable()) wipePrefixThread_.join();
}

void App::startUpdateCheck() {
    if (updateThread_.joinable()) return; // already started -- safe to call once
    updateThread_ = std::thread(&App::updateCheckThreadMain, this);
}

bool App::needsInstallerHandoff() const {
    return needsInstallerHandoff_.load();
}

bool App::needsUninstallHandoff() const {
    return needsUninstallHandoff_.load();
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

void App::requestUninstall() {
    if (uninstallThread_.joinable()) return; // already in progress -- ignore repeat clicks
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.uninstall.inProgress = true;
        snapshot_.uninstall.errorMessage.clear();
    }
    uninstallThread_ = std::thread(&App::uninstallThreadMain, this);
}

void App::uninstallThreadMain() {
    std::string channel;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        channel = snapshot_.settings.channel;
    }

    auto fail = [&](const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.uninstall.inProgress = false;
        snapshot_.uninstall.errorMessage = message;
    };

    // Any working TuxBloxInstaller build can run --uninstall (it doesn't
    // need to match the currently-installed version), so this always goes
    // for the channel's latest release rather than requiring a specific
    // one -- simpler than threading a version through, and this path is
    // only reached at all when nothing's cached locally yet (see
    // ensureInstallerBinary).
    std::optional<std::string> latestVersion;
    try {
        latestVersion = fetchLatestVersion(kSetupBaseUrl, channel, &uninstallCancel_);
    } catch (const std::exception& e) {
        fail(std::string("Couldn't reach tuxblox.net to prepare the uninstaller: ") + e.what());
        return;
    }
    if (!latestVersion.has_value()) {
        fail("No published release found for the current channel -- can't fetch an uninstaller.");
        return;
    }

    const std::string manifestUrl =
        std::string(kSetupBaseUrl) + "/v1/" + channel + "/" + *latestVersion + "/manifest.json";
    Manifest manifest;
    try {
        std::string json = fetchManifestJson(manifestUrl, &uninstallCancel_);
        manifest = parseManifest(json, kSetupBaseUrl);
    } catch (const std::exception& e) {
        fail(std::string("Couldn't fetch the release manifest: ") + e.what());
        return;
    }

    EnsureInstallerResult ensured = ensureInstallerBinary(manifest, installDir_, &uninstallCancel_, nullptr);
    if (!ensured.ok) {
        fail(ensured.errorMessage.empty() ? "Failed to prepare the uninstaller." : ensured.errorMessage);
        return;
    }

    installerHandoffPath_ = ensured.installerPath; // see its declaration comment on write-before-flag ordering
    needsUninstallHandoff_.store(true);
}

void App::requestWipePrefix() {
    if (wipePrefixThread_.joinable()) return; // already in progress -- ignore repeat clicks
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.wipePrefix.inProgress = true;
        snapshot_.wipePrefix.errorMessage.clear();
    }
    wipePrefixThread_ = std::thread(&App::wipePrefixThreadMain, this);
}

void App::wipePrefixThreadMain() {
    namespace fs = std::filesystem;
    const std::string runtimeDir = installDir_ + "/runtime";

    std::string error;
    std::error_code ec;
    if (fs::exists(runtimeDir, ec) && !ec) {
        // Wipe contents rather than the directory itself (fs::remove_all on
        // runtimeDir would also work since anything that needs it recreates
        // it lazily -- but leaving the empty directory in place matches
        // "wipe the contents of runtime/" literally, and means nothing
        // downstream has to distinguish "never launched yet" from "just
        // wiped").
        for (const auto& entry : fs::directory_iterator(runtimeDir, ec)) {
            if (ec) break;
            std::error_code removeEc;
            fs::remove_all(entry.path(), removeEc); // best-effort per entry
            if (removeEc && error.empty()) {
                error = "Failed to remove " + entry.path().string() + ": " + removeEc.message();
            }
        }
    }
    if (ec && error.empty()) {
        error = "Failed to read " + runtimeDir + ": " + ec.message();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.wipePrefix.inProgress = false;
    snapshot_.wipePrefix.errorMessage = error;
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
    std::string channel;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        channel = snapshot_.settings.channel;
    }

    auto report = [&](UpdateProgress p) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.update = p;
    };

    report({UpdatePhase::CheckingManifest, 0.0});

    std::optional<std::string> latestVersion;
    try {
        latestVersion = fetchLatestVersion(kSetupBaseUrl, channel, &updateCancel_);
    } catch (const std::exception& e) {
        report({UpdatePhase::Error, 0.0, e.what()});
        return;
    }
    if (!latestVersion.has_value()) {
        // No releases published for this channel yet -- there's nothing to
        // update to, so this isn't an error, just nothing further to do.
        report({UpdatePhase::UpToDate, 1.0});
        return;
    }

    auto result = runUpdateCheck(currentVersion_, kSetupBaseUrl, channel, *latestVersion,
        [&](UpdateProgress p) { report(p); },
        &updateCancel_, installDir_);
    if (result.needsHandoff) {
        installerHandoffPath_ = result.installerPath; // see installerHandoffPath()'s comment
        needsInstallerHandoff_.store(true);
    }
}

} // namespace tuxblox
