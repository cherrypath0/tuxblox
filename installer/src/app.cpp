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
#include "manifest.h"
#include "install_paths.h"
#include "installer_steps.h"
#include "desktop_shortcut.h"
#include "copyright_file.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {
constexpr uint64_t kMinFreeBytes = 3ULL * 1024 * 1024 * 1024; // 3GB, per README
constexpr const char* kSetupBaseUrl = "https://setup.tuxblox.net";

// Peak disk usage during install is roughly (compressed download + fully
// extracted tree) coexisting until the tarball is deleted post-extraction.
// 2.5x the manifest's declared artifact sizes is a conservative estimate of
// that peak; the fixed headroom covers the prefix/runtime scratch space.
constexpr double kPeakUsageFactor = 2.5;
constexpr uint64_t kHeadroomBytes = 500ULL * 1024 * 1024; // 500MB

// Best-effort cleanup of a partial install tree. Deliberately uses the
// non-throwing overload: if cleanup fails (e.g. permissions), we must not
// let that exception escape and overwrite the real install-failure message.
//
// NEVER called when isUpgrade is true: runInstall()'s own catch handler
// already removes whatever partial temp files *this run* created (each
// artifact's .<name>.part or .<name>.tar.part), and that is the full
// extent of safe cleanup on an upgrade -- wiping `dir` here would destroy
// the user's existing install (including runtime/, which holds their
// Roblox login session) over what might be nothing more than a network
// hiccup.
void cleanupBestEffort(const std::string& dir) {
    std::error_code ec;
    fs::remove_all(dir, ec);
}
} // namespace

App::App(std::string channel) : channel_(std::move(channel)) {}

App::~App() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void App::start() {
    thread_ = std::thread(&App::run, this);
}

void App::cancel() {
    cancelRequested_.store(true);
}

AppSnapshot App::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool App::readyToLaunch() const {
    return readyToLaunch_.load();
}

std::string App::launcherPath() const {
    return launcherPath_;
}

void App::run() {
    auto setPhase = [&](AppPhase phase) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.phase = phase;
    };
    auto setError = [&](const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.phase = AppPhase::Error;
        snapshot_.errorMessage = message;
    };

    try {
        setPhase(AppPhase::Init);

        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') {
            setError("HOME environment variable is not set.");
            return;
        }

        const std::string dir = installDir();
        // An existing install directory means this run is an upgrade in
        // place, not a fresh install -- runs the exact same pipeline, just
        // never wipes anything outside proton/ (and only after the
        // replacement is verified), and shows "Upgrading ..." wording
        // instead of refusing outright.
        const bool isUpgrade = fs::exists(dir);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.isUpgrade = isUpgrade;
        }
        if (!hasEnoughDiskSpace(home, kMinFreeBytes)) {
            setError("Not enough free disk space. TuxBlox requires at least 3GB free.");
            return;
        }
        if (cancelRequested_.load()) return;

        setPhase(AppPhase::FetchingManifest);
        auto latestVersion = fetchLatestVersion(kSetupBaseUrl, channel_, &cancelRequested_);
        if (cancelRequested_.load()) return;
        if (!latestVersion.has_value()) {
            setError("No releases available for the '" + channel_ + "' channel yet.");
            return;
        }
        const std::string manifestUrl =
            std::string(kSetupBaseUrl) + "/v1/" + channel_ + "/" + *latestVersion + "/manifest.json";
        std::string json = fetchManifestJson(manifestUrl, &cancelRequested_);
        Manifest manifest = parseManifest(json, kSetupBaseUrl);
        if (cancelRequested_.load()) return;

        // Second, precise disk-space check now that the manifest gives us the
        // real artifact sizes. (The flat kMinFreeBytes check above still runs
        // first as a cheap pre-network rejection.) Sums every artifact the
        // manifest lists, not a fixed set -- see runInstall's own comment.
        uint64_t artifactBytes = 0;
        for (const auto& [name, art] : manifest.artifacts) {
            artifactBytes += art.sizeBytes;
        }
        const uint64_t requiredBytes =
            static_cast<uint64_t>(static_cast<double>(artifactBytes) * kPeakUsageFactor) +
            kHeadroomBytes;
        if (!hasEnoughDiskSpace(home, requiredBytes)) {
            setError("Not enough free disk space. This install needs approximately " +
                     std::to_string(requiredBytes / (1024ULL * 1024ULL)) + " MB free.");
            return;
        }

        setPhase(AppPhase::Installing);
        auto outcome = runInstall(manifest,
            [&](const std::string& label, double percent) {
                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_.currentStepLabel = label;
                snapshot_.overallPercent = percent;
            },
            &cancelRequested_, isUpgrade);

        if (outcome.cancelled) {
            if (!isUpgrade) cleanupBestEffort(dir);
            return;
        }
        if (!outcome.ok) {
            if (!isUpgrade) cleanupBestEffort(dir);
            setError(outcome.errorMessage);
            return;
        }

        launcherPath_ = outcome.launcherPath;
        // Both take the resolved launcher *executable* path, never `dir` --
        // the launcher artifact is an archive (a Qt6 bundle directory), so
        // its binary is not directly at the install root.
        createDesktopShortcut(outcome.launcherPath); // best-effort -- see desktop_shortcut.h
        refreshUrlHandlers(outcome.launcherPath);    // best-effort -- see desktop_shortcut.h
        writeCopyrightFile(dir);    // best-effort -- see copyright_file.h
        readyToLaunch_.store(true);
        setPhase(AppPhase::Done);
    } catch (const std::exception& e) {
        // A cancel can unwind as an exception (fetchManifestJson aborts its
        // transfer by throwing). That's user-initiated, not a failure to
        // report -- stay silent, matching the outcome.cancelled path above.
        if (cancelRequested_.load()) return;
        setError(e.what());
    }
}

} // namespace tuxblox
