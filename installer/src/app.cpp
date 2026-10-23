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
#include "release_file.h"
#include <sstream>
#include <fstream>
#include "install_paths.h"
#include "installer_steps.h"
#include "desktop_shortcut.h"
#include "copyright_file.h"
#include "version.h"
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string readWholeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path);
    std::ostringstream body;
    body << in.rdbuf();
    return body.str();
}

} // namespace

namespace {
constexpr uint64_t kMinFreeBytes = 3ULL * 1024 * 1024 * 1024; // 3GB, per README
// Where TuxBlox downloads itself from.
//
// Overridable ONLY in a build configured with -DTUXBLOX_TEST_SETUP_URL=ON, which nothing shipped
// ever is. An environment variable that redirects this in a released binary would let anything
// able to set one -- a planted .desktop entry, a modified shell profile -- serve its own "update"
// and have TuxBlox install and run it. The end-to-end test needs the seam; a user must not have it.
#ifdef TUXBLOX_TEST_SETUP_URL
const char* setupBaseUrl() {
    const char* override = std::getenv("TUXBLOX_SETUP_URL");
    return (override && *override) ? override : "https://setup.tuxblox.net";
}
#else
const char* setupBaseUrl() { return "https://setup.tuxblox.net"; }
#endif

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

// Session logs describe runs of the version being replaced, nothing reads them back, and one per
// launch is how an install quietly reaches hundreds of files. Best effort throughout: losing a log
// is not worth failing an upgrade over.
void clearSessionLogs(const std::string& dir) {
    std::error_code ec;
    std::vector<fs::path> entries;
    for (const auto& entry : fs::directory_iterator(dir + "/logs", ec)) entries.push_back(entry.path());
    for (const auto& entry : entries) fs::remove_all(entry, ec);
}
} // namespace

App::App(std::string channel, ReleaseSource source, std::string installDirOverride)
    : channel_(std::move(channel)), source_(std::move(source)),
      installDirOverride_(std::move(installDirOverride)) {}

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

        const std::string dir = installDirOverride_.empty() ? installDir() : installDirOverride_;
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
        if (!hasEnoughDiskSpace(existingAncestor(dir), kMinFreeBytes)) {
            setError("Not enough free disk space. TuxBlox requires at least 3GB free.");
            return;
        }
        if (cancelRequested_.load()) return;

        setPhase(AppPhase::FetchingManifest);
        // Three ways in, and which one is in play was decided by the command line. The default is
        // the channel's newest release: this binary's own version is its COMPONENT version now, not
        // a release, so installing "the version it was built for" would ask for something that was
        // never published.
        std::string releaseJson;
        try {
            if (source_.kind == ReleaseSource::Kind::File) {
                // The launcher resolved this and wrote the bytes it read. Taking them rather than
                // re-fetching is the point: two reads can straddle a pointer move, and the record
                // written at the end of this install would then name a release nobody decided on.
                releaseJson = readWholeFile(source_.path);
            } else {
                std::string release = source_.release;
                if (release.empty()) {
                    auto named = fetchLatestRelease(setupBaseUrl(), channel_, &cancelRequested_);
                    if (cancelRequested_.load()) return;
                    if (!named.has_value()) {
                        setError("No releases available for the '" + channel_ + "' channel yet.");
                        return;
                    }
                    release = *named;
                }
                releaseJson = fetchManifestJson(
                    releaseFileUrl(setupBaseUrl(), channel_, release), &cancelRequested_);
            }
        } catch (const std::exception& e) {
            // A named release that is not published reads as a raw 404, which tells the user
            // nothing. The newest-release path has no number of its own to be missing -- the
            // pointer just named it -- so a failure there is a genuine one. A cancel mid-transfer
            // also arrives as an exception and must stay silent.
            if (source_.kind != ReleaseSource::Kind::Named || cancelRequested_.load()) throw;
            setError("Release " + source_.release + " is not published on the '" + channel_ +
                     "' channel. Leave --release off to install the newest release instead. (" +
                     e.what() + ")");
            return;
        }
        if (cancelRequested_.load()) return;

        ReleaseFile release;
        try {
            release = parseReleaseFile(releaseJson);
        } catch (const std::exception& e) {
            setError(std::string("That release cannot be read: ") + e.what());
            return;
        }
        // The handoff file has been read; it is scratch the launcher wrote for this one run.
        if (source_.kind == ReleaseSource::Kind::File) {
            std::error_code ec;
            fs::remove(source_.path, ec);
        }


        // Second, precise disk-space check now that the manifest gives us the
        // real artifact sizes. (The flat kMinFreeBytes check above still runs
        // first as a cheap pre-network rejection.) Sums every artifact the
        // manifest lists, not a fixed set -- see runInstall's own comment.
        uint64_t artifactBytes = 0;
        for (const auto& [name, component] : release.components) {
            const std::optional<ComponentBytes>& bytes =
                std::string(hostArch()) == "arm64" ? component.arm64 : component.x86_64;
            if (bytes.has_value()) artifactBytes += bytes->size;
        }
        const uint64_t requiredBytes =
            static_cast<uint64_t>(static_cast<double>(artifactBytes) * kPeakUsageFactor) +
            kHeadroomBytes;
        if (!hasEnoughDiskSpace(existingAncestor(dir), requiredBytes)) {
            setError("Not enough free disk space. This install needs approximately " +
                     std::to_string(requiredBytes / (1024ULL * 1024ULL)) + " MB free.");
            return;
        }

        setPhase(AppPhase::Installing);
        auto outcome = runInstall(release, setupBaseUrl(),
            [&](const std::string& label, double percent) {
                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_.currentStepLabel = label;
                snapshot_.overallPercent = percent;
            },
            &cancelRequested_, isUpgrade, installDirOverride_,
            "https://setup.rbxcdn.com/RobloxPlayerInstaller.exe",
            "https://setup.rbxcdn.com/RobloxStudioInstaller.exe",
            // The bytes this run installed from, recorded last as the install's identity.
            releaseJson);

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
        // the launcher artifact is an archive (a bundle directory), so
        // its binary is not directly at the install root.
        createDesktopShortcut(outcome.launcherPath); // best-effort -- see desktop_shortcut.h
        refreshUrlHandlers(outcome.launcherPath);    // best-effort -- see desktop_shortcut.h
        writeCopyrightFile(dir);    // best-effort -- see copyright_file.h
        // Only on an upgrade: a fresh install has no logs, and this is the one moment at which every
        // log present belongs to a version that is no longer installed.
        if (isUpgrade) clearSessionLogs(dir);

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
