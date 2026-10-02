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
#include "child_environment.h"
#include "prefix_user.h"
#include "root_guard.h"
#include "checksum.h"
#include "container_env.h"
#include "downloader.h"
#include "manifest.h"
#include "prefix_session.h"
#include "roblox_deploy.h"
#include "tar_extract.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace tuxblox {

namespace fs = std::filesystem;

namespace {
constexpr const char* kSetupBaseUrl = "https://setup.tuxblox.net";

// "%Y-%m-%dT%H:%M:%SZ" -- same strftime/gmtime_r pattern as
// process_launcher.cpp's logTimestamp(), but UTC and ISO-8601 for
// versions.json's InstalledVersion::installedAt.
std::string isoNowUtc() {
    std::time_t t = std::time(nullptr);
    std::tm tmBuf{};
    gmtime_r(&t, &tmBuf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return buf;
}

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
    // Prefix-first, not versions.json-first: a deleted or stale manifest
    // must not make an already-installed Roblox look uninstalled (the
    // Start tab would offer "Install & Launch" for something that's
    // sitting right there in the prefix).
    snapshot_.versions = loadInstalledVersions(installDir_);
    // Safe to call unlocked here: the constructor runs before
    // startUpdateCheck() spawns any other thread, so nothing else can be
    // concurrently calling getenv() yet. See applyGlobalEnvVars()'s own
    // comment for why that ordering matters everywhere else it's called.
    applyEnvVars(snapshot_.settings);

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

    // Taken once here rather than only on the poll thread, so the first frame
    // already shows Stop for a session that was running before the launcher was
    // opened instead of flickering from Launch a second later.
    snapshot_.sessions = prefixSessions(installDir_ + "/runtime/pfx");
    sessionPollThread_ = std::thread(&App::sessionPollThreadMain, this);
}

App::~App() {
    // Let an in-flight update download (the installer binary, or -- before
    // this handed off to it -- Proton) abort promptly instead of blocking
    // this join for however long the download would otherwise take -- the
    // window is already gone by the time the destructor runs. See Finding
    // 2, 2026-07-28 final review.
    updateCancel_.store(true);
    uninstallCancel_.store(true);
    versionInstallCancel_.store(true);
    sessionPollStop_.store(true);
    if (sessionPollThread_.joinable()) sessionPollThread_.join();
    if (stopPlayerThread_.joinable()) stopPlayerThread_.join();
    if (stopStudioThread_.joinable()) stopStudioThread_.join();
    if (updateThread_.joinable()) updateThread_.join();
    if (updateApplyThread_.joinable()) updateApplyThread_.join();
    if (uninstallThread_.joinable()) uninstallThread_.join();
    if (wipePrefixThread_.joinable()) wipePrefixThread_.join();
    if (versionInstallThread_.joinable()) versionInstallThread_.join();
}

void App::startUpdateCheck() {
    if (updateThread_.joinable()) return; // already started -- safe to call once
    updateThread_ = std::thread(&App::updateCheckThreadMain, this);
}

bool App::waitForUpdateCheck(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
        if (needsInstallerHandoff_.load()) return true;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const UpdatePhase phase = snapshot_.update.phase;
            if (phase == UpdatePhase::UpToDate || phase == UpdatePhase::Error) return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
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

    bool closeWindow;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closeWindow = snapshot_.settings.minimizeToBackground;
    }

    const char* targetArg = (target == LaunchTarget::Player) ? "player" : "studio";
    std::vector<char*> argv = {const_cast<char*>(launcherExePath_.c_str()), const_cast<char*>("--watch-launch"),
                               const_cast<char*>(targetArg)};
    // Carried across every re-exec, or a root run that was allowed here is refused by the copy of ourselves that does the launching.
    if (allowRoot()) argv.push_back(const_cast<char*>("--allow-root"));
    argv.push_back(nullptr);
    // Wine is started by the process this becomes, so it must not inherit the interface libraries' font and settings paths
    ChildEnvironment environment;

    pid_t pid = fork();
    if (pid < 0) return; // fork failed -- nothing else to do, stay open
    if (pid == 0) {
        // Double-fork detach: the immediate child exits right away, the
        // grandchild (the actual --watch-launch process) is re-parented to
        // init so it outlives this whole launcher cleanly, with no zombie
        // left behind.
        pid_t inner = fork();
        if (inner == 0) {
            setsid();
            execve(launcherExePath_.c_str(), argv.data(), environment.envp());
            _exit(127); // only reached if execve itself failed
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0); // reap the immediate child; it exits almost instantly

    // Off means the window stays open and can launch again, so the "already spawned one" guard above must not latch
    if (closeWindow) shouldQuit_.store(true);
}

int App::requestTerminateProcesses() {
    // Sessions first, so each one ends through the compatibility layer and
    // reports success instead of being killed underneath it, which showed the
    // user a crash window for something they asked for.
    stopPrefixSessions(installDir_, LaunchTarget::Player);
    stopPrefixSessions(installDir_, LaunchTarget::Studio);
    return terminatePrefixProcesses(installDir_ + "/runtime/pfx");
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
        // Mutual exclusion with an in-progress version install (Finding 4b,
        // 2026-08-16 final review): both touch files under runtime/
        // concurrently otherwise -- wiping the prefix out from under an
        // in-flight extractZip(), or reviving a directory the install just
        // finished deleting-and-recreating. Same "phase" check
        // requestInstallVersion() already uses for its own re-entrancy guard.
        VersionInstallPhase installPhase = snapshot_.versionInstall.phase;
        bool installRunning = installPhase != VersionInstallPhase::Idle &&
                               installPhase != VersionInstallPhase::Done &&
                               installPhase != VersionInstallPhase::Error;
        if (installRunning) return;
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

    // Wiping runtime/ deletes every Roblox version directory it contained --
    // reset versions.json (and the in-memory snapshot below) to match,
    // otherwise the Versions tab / Start tab keep showing entries for
    // versions that no longer exist on disk (Finding 4a, 2026-08-16 final
    // review). Disk I/O outside the lock -- same convention as
    // updateSettings().
    VersionsManifest emptyVersions{};
    saveVersionsManifest(installDir_, emptyVersions);

    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.wipePrefix.inProgress = false;
    snapshot_.wipePrefix.errorMessage = error;
    snapshot_.versions = emptyVersions;
}

void App::sessionPollThreadMain() {
    // Once a second, not on the interface's own 100 ms tick: this walks /proc
    // and reads a file per process, which is far too much work to do ten times
    // a second for a button label.
    while (!sessionPollStop_.load()) {
        for (int waited = 0; waited < 1000 && !sessionPollStop_.load(); waited += 100) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (sessionPollStop_.load()) return;

        PrefixSessions sessions = prefixSessions(installDir_ + "/runtime/pfx");
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.sessions = sessions;
        if (sessions.player == 0) snapshot_.stoppingPlayer = false;
        if (sessions.studio == 0) snapshot_.stoppingStudio = false;
    }
}

void App::requestStopSessions(LaunchTarget target) {
    const bool player = target == LaunchTarget::Player;
    std::atomic<bool>& busy = player ? stopPlayerBusy_ : stopStudioBusy_;
    if (busy.exchange(true)) return; // a stop for this app is already running
    std::thread& thread = player ? stopPlayerThread_ : stopStudioThread_;
    if (thread.joinable()) thread.join(); // an earlier stop has finished -- reclaim it before reusing the slot
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (player) {
            snapshot_.stoppingPlayer = true;
        } else {
            snapshot_.stoppingStudio = true;
        }
    }
    thread = std::thread(&App::stopSessionsThreadMain, this, target);
}

void App::stopSessionsThreadMain(LaunchTarget target) {
    stopPrefixSessions(installDir_, target);

    PrefixSessions sessions = prefixSessions(installDir_ + "/runtime/pfx");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.sessions = sessions;
        if (target == LaunchTarget::Player) {
            snapshot_.stoppingPlayer = false;
        } else {
            snapshot_.stoppingStudio = false;
        }
    }
    (target == LaunchTarget::Player ? stopPlayerBusy_ : stopStudioBusy_).store(false);
}

AppSnapshot App::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void App::updateSettings(Settings settings) {
    saveSettings(installDir_, settings);
    bool envChanged;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // The card picker feeds the same environment, so a change to it has
        // to re-apply too -- otherwise picking a card would do nothing until
        // the launcher was restarted.
        envChanged = snapshot_.settings.envVars != settings.envVars ||
                     snapshot_.settings.gpu != settings.gpu;
        snapshot_.settings = settings;
    }
    if (envChanged) {
        applyEnvVars(settings);
    }
}

void App::requestUpdateNow() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!snapshot_.updateAvailableVersion.has_value()) return;
    }
    if (updateApplyStarted_.exchange(true)) return; // already fetching -- ignore repeat clicks
    // A failed attempt clears the flag again, so this joins that finished thread before replacing it.
    if (updateApplyThread_.joinable()) updateApplyThread_.join();
    updateApplyThread_ = std::thread(&App::updateApplyThreadMain, this);
}

void App::updateApplyThreadMain() {
    Manifest manifest;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        manifest = pendingUpdateManifest_;
    }
    if (!prepareInstallerHandoff(manifest)) {
        updateApplyStarted_.store(false);
        return;
    }
    needsInstallerHandoff_.store(true);
}

bool App::prepareInstallerHandoff(const Manifest& manifest) {
    EnsureInstallerResult ensured = ensureInstallerBinary(manifest, installDir_, &updateCancel_,
        [this](UpdateProgress p) {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.update = p;
        });
    if (!ensured.ok) return false;

    installerHandoffPath_ = ensured.installerPath; // see its declaration comment on write-before-flag ordering
    return true;
}

void App::dismissUpdateNotification() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.updateAvailableVersion.reset();
}

void App::applyEnvVars(const Settings& settings) {
    // setenv()/getenv() are not thread-safe in glibc (Finding 6,
    // 2026-07-28 final review) -- this process does have other threads
    // that may call getenv() (the update-check thread, inside curl).
    // Environment Variables are deliberately applied to the launcher's own
    // process too (real "export" semantics -- see the settings design doc),
    // so that hazard can't be avoided by scoping to a forked child. The
    // constructor call site is race-free (nothing else is running yet);
    // later calls from updateSettings() (user edits, on the render thread)
    // accept the same small, already-documented race rather than adding
    // cross-thread coordination for a rare, user-initiated edit.
    for (const auto& kv : launchEnvPairs(settings)) {
        auto pos = kv.find('=');
        setenv(kv.substr(0, pos).c_str(), kv.substr(pos + 1).c_str(), 1);
    }
}

void App::updateCheckThreadMain() {
    std::string channel;
    bool autoUpdate;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        channel = snapshot_.settings.channel;
        autoUpdate = snapshot_.settings.autoUpdate;
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
        // Deliberately gated on autoUpdate alone, even when
        // result.protonMissing is true (no Proton install at all, e.g. a
        // first run) -- an earlier version of this code always bypassed the
        // toggle for that case ("nothing to opt out of if nothing's
        // installed"), but that meant the launcher silently closed and
        // handed off to the installer on every single startup, with no
        // window ever shown, for anyone with autoUpdate off and no Proton
        // yet -- surprising and impossible to interrupt if the user actually
        // wanted to look at the app first. StartTab's "Install & Launch"
        // button (see start_tab.cpp) already covers this case on demand: it
        // calls requestLaunch(), whose existing bootstrap fallback chain
        // (process_launcher.cpp's resolveOrBootstrapExePath) installs Proton
        // the moment the user actually tries to launch something, so this
        // startup-time path installing it unprompted is redundant, not just
        // surprising.
        // A mixed install ignores the Auto-Update setting: its pieces do not
        // agree on one build, which is broken rather than out of date, and
        // leaving it running is not a state the user chose. An ordinary
        // available update still waits to be asked for, below.
        if (autoUpdate || result.mixedInstall) {
            if (!prepareInstallerHandoff(result.manifest)) return;
            needsInstallerHandoff_.store(true);
        } else {
            // Nothing is fetched and nothing on disk is touched on this path: an update that is only being offered has to leave the install exactly as it was, or the next check reads it as broken and repairs it without asking.
            // pendingUpdateManifest_ is written under mutex_, in the same critical section as updateAvailableVersion -- a UI-thread caller that has observed updateAvailableVersion through snapshot() is guaranteed to see it too, so requestUpdateNow() needs nothing but mutex_ to pick it up.
            std::lock_guard<std::mutex> lock(mutex_);
            pendingUpdateManifest_ = result.manifest;
            snapshot_.updateAvailableVersion = *latestVersion;
            // Without this the phase stays where runUpdateCheck()'s last report() left it -- CheckingManifest -- forever, since nothing else in this function reports again on this branch. StartTab::updateFromSnapshot() treats that as "updating" and hides the Launch buttons in favour of the progress bar for as long as it is true, so the Home tab would sit there with no way to launch anything. The branch above needs no such reset: it sets needsInstallerHandoff_, which MainWindow::poll() acts on by closing the window within one tick.
            snapshot_.update = {UpdatePhase::UpToDate, 1.0};
        }
    }
}

void App::requestInstallVersion(LaunchTarget target, VersionSelectMode mode, const std::string& channel,
                                 const std::string& manualHash) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // joinable() only means "not yet joined/detached" -- NOT "still
        // running" (the std::thread object stays joinable after its
        // function returns, until something joins it). So the real
        // "already in progress" signal is the tracked phase, not the
        // thread handle -- otherwise every install after the first one
        // ever run would silently no-op forever.
        VersionInstallPhase phase = snapshot_.versionInstall.phase;
        bool stillRunning = phase != VersionInstallPhase::Idle && phase != VersionInstallPhase::Done &&
                             phase != VersionInstallPhase::Error;
        if (stillRunning) return; // genuinely in progress -- ignore repeat clicks
        // Mutual exclusion with an in-progress Wipe Prefix (Finding 4b,
        // 2026-08-16 final review) -- see requestWipePrefix()'s matching guard.
        if (snapshot_.wipePrefix.inProgress) return;
        snapshot_.versionInstall = VersionInstallProgress{VersionInstallPhase::ResolvingVersion, 0.0, "", target};
    }
    // The previous run (if any) has already finished per the phase check
    // above, so this join is a formality -- it just reaps the finished
    // std::thread object back to a non-joinable state before reassigning.
    if (versionInstallThread_.joinable()) versionInstallThread_.join();
    versionInstallCancel_.store(false);
    versionInstallThread_ = std::thread(&App::versionInstallThreadMain, this, target, mode, channel, manualHash);
}

void App::versionInstallThreadMain(LaunchTarget target, VersionSelectMode mode, std::string channel,
                                    std::string manualHash) {
    auto report = [&](VersionInstallProgress p) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.versionInstall = p;
    };
    auto fail = [&](const std::string& message) {
        report({VersionInstallPhase::Error, 0.0, message, target});
    };

    const char* binaryType = robloxBinaryType(target);
    // Canonical form, so the version list doesn't show "ZCanary" and
    // "zcanary" as two different channels.
    channel = normalizeChannel(channel);

    std::string hash;
    try {
        if (mode == VersionSelectMode::ManualHash) {
            hash = manualHash;
        } else if (mode == VersionSelectMode::Latest) {
            hash = parseClientVersionHash(fetchClientVersionJson(binaryType, channel, &versionInstallCancel_));
        } else { // Previous
            std::string latestHash =
                parseClientVersionHash(fetchClientVersionJson(binaryType, channel, &versionInstallCancel_));
            std::string history = fetchDeployHistory(channel, &versionInstallCancel_);
            auto prev = previousVersionFromDeployHistory(history, binaryType, latestHash);
            if (!prev.has_value()) {
                fail("No earlier version found in Roblox's deploy history for this channel. "
                      "Roblox hides the version list for its private channels, so this only "
                      "works on the live channel.");
                return;
            }
            hash = *prev;
        }
    } catch (const std::exception& e) {
        const std::string detail = e.what();
        if (detail.find("401") != std::string::npos) {
            fail("Roblox does not allow this channel to be looked up, so TuxBlox cannot find its "
                  "latest version. Type an exact version instead, or use the live channel.");
        } else {
            fail(std::string("Couldn't resolve a version: ") + detail);
        }
        return;
    }
    if (hash.empty()) {
        fail("No version hash to install.");
        return;
    }

    report({VersionInstallPhase::DownloadingManifest, 0.05, "", target});

    std::vector<PackageEntry> packages;
    try {
        static const char* kMirrors[] = {"setup.rbxcdn.com", "setup-aws.rbxcdn.com"};
        std::string manifestText;
        std::string lastError;
        bool ok = false;
        for (const char* mirror : kMirrors) {
            try {
                manifestText = fetchText(setupCdnUrl(mirror, channel, hash, "rbxPkgManifest.txt"),
                                          &versionInstallCancel_);
                ok = true;
                break;
            } catch (const std::exception& e) {
                lastError = e.what();
            }
        }
        if (!ok) throw std::runtime_error(lastError);
        packages = parsePackageManifest(manifestText);
    } catch (const std::exception& e) {
        fail(std::string("Couldn't fetch the package manifest (this version may have been purged from "
                          "Roblox's CDN): ") + e.what());
        return;
    }

    const std::string versionDir =
        prefixUserDir(installDir_) + "/AppData/Local/Roblox/Versions/" + hash;
    const std::string stagingDir = installDir_ + "/RobloxPackageStaging/" + hash;
    std::error_code ec;
    fs::create_directories(stagingDir, ec);

    report({VersionInstallPhase::DownloadingPackages, 0.1, "", target});

    for (size_t i = 0; i < packages.size(); ++i) {
        const auto& pkg = packages[i];
        auto subdir = packageInstallSubdir(pkg.name, target);
        if (!subdir.has_value()) continue; // unknown package -- skip (see packageInstallSubdir's doc)

        const std::string destZip = stagingDir + "/" + pkg.name;
        static const char* kMirrors[] = {"setup.rbxcdn.com", "setup-aws.rbxcdn.com"};
        DownloadOutcome outcome{DownloadResult::Failed, "no mirror attempted"};
        for (const char* mirror : kMirrors) {
            outcome = downloadFile(setupCdnUrl(mirror, channel, hash, pkg.name), destZip,
                [&](uint64_t now, uint64_t total) {
                    double pkgFraction = total > 0 ? static_cast<double>(now) / total : 0.0;
                    double overall = 0.1 + 0.6 * ((i + pkgFraction) / std::max<size_t>(1, packages.size()));
                    report({VersionInstallPhase::DownloadingPackages, overall, "", target});
                },
                &versionInstallCancel_);
            if (outcome.result == DownloadResult::Ok) break;
        }
        if (outcome.result != DownloadResult::Ok) {
            // Also remove versionDir, not just stagingDir (Finding 6,
            // 2026-08-16 final review): an earlier package in this same loop
            // may have already extracted into it before this one failed,
            // which would otherwise leave a partial, unregistered version
            // directory on disk. fs::remove_all is a no-op if it was never
            // created yet.
            fs::remove_all(versionDir, ec);
            fs::remove_all(stagingDir, ec);
            fail("Failed to download " + pkg.name + ": " + outcome.errorMessage);
            return;
        }
        if (md5File(destZip) != pkg.md5) {
            fs::remove_all(versionDir, ec); // see the matching comment above
            fs::remove_all(stagingDir, ec);
            fail("Checksum mismatch for " + pkg.name + " -- refusing to install a corrupted package.");
            return;
        }

        report({VersionInstallPhase::Extracting, 0.7 + 0.25 * (static_cast<double>(i) / packages.size()),
                "", target});
        const std::string destDir = versionDir + (subdir->empty() ? "" : ("/" + *subdir));
        try {
            extractZip(destZip, destDir);
        } catch (const std::exception& e) {
            fs::remove_all(versionDir, ec);
            fs::remove_all(stagingDir, ec);
            fail(std::string("Failed to extract ") + pkg.name + ": " + e.what());
            return;
        }
    }
    fs::remove_all(stagingDir, ec);

    // The official Roblox installer's bootstrap path writes this file into
    // every version directory -- without it, Roblox's client can't find its
    // content folder and fails at startup (Finding 2, 2026-08-16 final
    // review). Confirmed byte-for-byte against a real installed version on
    // this machine. Written with the same std::ofstream idiom as
    // settings.cpp/versions_manifest.cpp's own file writes, but treated as a
    // hard failure (not best-effort) since a version installed without it is
    // unusable.
    {
        std::ofstream appSettings(versionDir + "/AppSettings.xml", std::ios::binary);
        if (!appSettings) {
            fs::remove_all(versionDir, ec);
            fail("Failed to write AppSettings.xml into " + versionDir);
            return;
        }
        appSettings << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<Settings>\n"
                       "\t<ContentFolder>content</ContentFolder>\n"
                       "\t<BaseUrl>http://www.roblox.com</BaseUrl>\n"
                       "</Settings>";
        if (!appSettings) {
            fs::remove_all(versionDir, ec);
            fail("Failed to write AppSettings.xml into " + versionDir);
            return;
        }
    }

    // Re-loaded from disk immediately before mutating/saving, rather than
    // mutating the in-memory snapshot_.versions copy directly (Finding 5,
    // 2026-08-16 final review): a detached --watch-launch process can run
    // registerBootstrappedVersion() concurrently and write versions.json
    // independently (it deliberately doesn't take this process's lock --
    // see main.cpp), so saving a possibly-stale in-memory copy can silently
    // clobber what that process just wrote. This narrows the race window to
    // "load-then-save" instead of "constructor-load-then-save-anytime-later".
    VersionsManifest fresh = loadInstalledVersions(installDir_);
    {
        AppVersions& av = appVersionsFor(fresh, target);
        // loadInstalledVersions() already picked this version up off the
        // prefix (it was extracted above), so record the metadata on that
        // entry instead of appending a duplicate.
        auto it = std::find_if(av.installed.begin(), av.installed.end(),
                                [&](const InstalledVersion& v) { return v.hash == hash; });
        if (it == av.installed.end()) {
            av.installed.push_back({hash, channel, isoNowUtc()});
        } else {
            it->channel = channel;
            it->installedAt = isoNowUtc();
        }
        if (av.activeHash.empty()) av.activeHash = hash; // first version for this app type -- pin it
    }
    saveVersionsManifest(installDir_, fresh);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.versions = fresh; // so the UI reflects the merged-on-disk result
    }
    report({VersionInstallPhase::Done, 1.0, "", target});
}

void App::requestSetActiveVersion(LaunchTarget target, const std::string& hash) {
    // Re-loaded from disk rather than mutated from the in-memory snapshot_
    // copy -- see versionInstallThreadMain's matching comment (Finding 5,
    // 2026-08-16 final review).
    VersionsManifest fresh = loadInstalledVersions(installDir_);
    AppVersions& av = appVersionsFor(fresh, target);
    bool installed = std::any_of(av.installed.begin(), av.installed.end(),
                                  [&](const InstalledVersion& v) { return v.hash == hash; });
    if (!installed) return;
    av.activeHash = hash;
    saveVersionsManifest(installDir_, fresh);
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.versions = fresh;
}

void App::requestDeleteVersion(LaunchTarget target, const std::string& hash) {
    // Re-loaded from disk rather than mutated from the in-memory snapshot_
    // copy -- see versionInstallThreadMain's matching comment (Finding 5,
    // 2026-08-16 final review).
    VersionsManifest fresh = loadInstalledVersions(installDir_);
    AppVersions& av = appVersionsFor(fresh, target);
    auto it = std::find_if(av.installed.begin(), av.installed.end(),
                            [&](const InstalledVersion& v) { return v.hash == hash; });
    if (it == av.installed.end()) return;
    av.installed.erase(it);
    // The version in use can be deleted like any other. Dropping the pin
    // lets the reconcile below choose the newest of whatever is left, or
    // leave nothing pinned when this was the last one -- in which case the
    // next launch installs Roblox again.
    if (hash == av.activeHash) av.activeHash.clear();

    const std::string versionDir = prefixVersionsDir(installDir_) + "/" + hash;
    std::error_code ec;
    fs::remove_all(versionDir, ec); // best-effort

    reconcileWithPrefix(installDir_, fresh);
    saveVersionsManifest(installDir_, fresh);
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.versions = fresh;
}

} // namespace tuxblox
