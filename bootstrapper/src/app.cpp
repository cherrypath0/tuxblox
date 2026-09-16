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
#include "checksum.h"
#include "downloader.h"
#include "install_plan.h"
#include "roblox_deploy.h"
#include "tar_extract.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// The package set a Studio install actually downloads. Preview walks this
// list so the window shows realistic names without contacting anything.
const std::vector<std::string>& previewPackages() {
    static const std::vector<std::string> kPackages = {
        "RobloxApp.zip",
        "redist.zip",
        "shaders.zip",
        "ssl.zip",
        "content-avatar.zip",
        "content-fonts.zip",
        "content-sky.zip",
        "content-sounds.zip",
        "content-textures2.zip",
        "extracontent-luapackages.zip",
        "extracontent-translations.zip",
        "BuiltInPlugins.zip",
        "ApplicationConfig.zip",
        "Plugins.zip",
        "StudioFonts.zip",
        "Qml.zip",
    };
    return kPackages;
}

// Enough connections to keep the link busy without hammering Roblox's CDN.
constexpr size_t kDownloadWorkers = 4;

const char* titleFor(Mode mode) {
    switch (mode) {
        case Mode::Preview: return "Preview Mode";
        case Mode::Update:  return "Checking for updates...";
        case Mode::Install: return "Installing Roblox";
        case Mode::None:    break;
    }
    return "TuxBlox";
}

void nap(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

} // namespace

App::App(Mode mode, Config config) : mode_(mode), config_(std::move(config)) {
    snapshot_.status = titleFor(mode);
}

App::~App() {
    cancel();
    if (thread_.joinable()) thread_.join();
}

void App::start() {
    if (thread_.joinable()) return;
    thread_ = std::thread(&App::run, this);
}

void App::cancel() { cancelled_ = true; }

Snapshot App::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool App::finished() const { return finished_; }

bool App::holdsOpenWhenDone() const { return mode_ == Mode::Preview; }

void App::setStatus(const std::string& status, double percent) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.phase = Phase::Working;
    snapshot_.status = status;
    snapshot_.overallPercent = percent;
}

void App::finish(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.phase = Phase::Done;
    snapshot_.status = message;
    snapshot_.overallPercent = 100.0;
    finished_ = true;
}

void App::fail(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.phase = Phase::Error;
    snapshot_.errorMessage = message;
    finished_ = true;
}

void App::run() {
    switch (mode_) {
        case Mode::Preview:
            runPreview();
            break;
        case Mode::Install:
            runInstall(false);
            break;
        case Mode::Update:
            runInstall(true);
            break;
        case Mode::None:
            fail("Nothing to do.");
            break;
    }
}

// No downloader and no filesystem work is constructed anywhere on this path,
// so preview cannot touch the network or the disk even if it is wrong.
void App::runPreview() {
    // Every line says so, because the steps below name real packages and
    // would otherwise read as a download that is actually happening.
    const std::string prefix = "Preview Mode - ";
    const auto& packages = previewPackages();
    // Two opening steps, then three per package.
    const double steps = 2.0 + static_cast<double>(packages.size()) * 3.0;
    double done = 0.0;
    auto advance = [&](const std::string& status, int pauseMs) {
        if (cancelled_) return false;
        done += 1.0;
        setStatus(prefix + status, done / steps * 100.0);
        nap(pauseMs);
        return !cancelled_;
    };

    if (!advance("Contacting Roblox servers", 700)) return;
    if (!advance("Fetching the package list", 500)) return;

    for (const auto& package : packages) {
        if (!advance("Downloading " + package, 220)) return;
        if (!advance("Extracting " + package, 140)) return;
        if (!advance("Installing " + package, 90)) return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_.phase = Phase::Done;
        snapshot_.status = prefix + "Finished";
        snapshot_.overallPercent = 100.0;
    }
    finished_ = true;
}


// Resolves the version to install, then downloads, verifies and extracts
// every package into <installDir>/<hash>.
void App::runInstall(bool skipIfPresent) {
    const std::vector<std::string> mirrors = mirrorsFor(config_);
    const bool updating = mode_ == Mode::Update;

    std::string hash = config_.hash;
    if (hash.empty()) {
        setStatus(updating ? "Checking for updates..." : "Contacting Roblox servers", 2.0);
        try {
            hash = parseClientVersionHash(fetchClientVersionJson(
                robloxBinaryType(config_.app), config_.channel, &cancelled_));
        } catch (const std::exception& e) {
            const std::string detail = e.what();
            if (detail.find("401") != std::string::npos) {
                fail("Roblox does not allow this channel to be looked up. Name an exact "
                      "version instead, or use the live channel.");
            } else {
                fail(std::string("Could not work out which version to install: ") + detail);
            }
            return;
        }
    }
    if (cancelled_) return;

    const std::string target = versionDir(config_.installDir, hash);
    // Checked by executable, not by directory: a folder left behind by an
    // interrupted install has to be finished, not mistaken for a version.
    // A named version is checked against either app, since its hash already
    // decided which one it is.
    const bool present = config_.hash.empty() ? versionIsInstalled(target, config_.app)
                                               : versionIsInstalledForAnyApp(target);
    if (skipIfPresent && present) {
        finish("Roblox is up to date");
        return;
    }
    // A leftover directory would otherwise collide with the rename at the end.
    if (fs::exists(target)) {
        std::error_code stale;
        fs::remove_all(target, stale);
    }

    setStatus("Fetching the package list", 5.0);
    std::vector<PackageEntry> packages;
    try {
        std::string manifestText;
        std::string lastError = "no mirror attempted";
        bool ok = false;
        for (const auto& mirror : mirrors) {
            try {
                manifestText = fetchText(
                    setupCdnUrl(mirror, config_.channel, hash, "rbxPkgManifest.txt"), &cancelled_);
                ok = true;
                break;
            } catch (const std::exception& e) {
                lastError = e.what();
            }
        }
        if (!ok) throw std::runtime_error(lastError);
        packages = parsePackageManifest(manifestText);
    } catch (const std::exception& e) {
        fail(std::string("Could not fetch the package list (Roblox may have removed this "
                          "version): ") + e.what());
        return;
    }
    if (cancelled_) return;

    std::error_code error;
    const std::string staging = target + ".partial";
    fs::remove_all(staging, error);
    fs::create_directories(staging, error);
    if (error) {
        fail("Could not create " + staging + ": " + error.message());
        return;
    }

    // Anything that fails from here leaves nothing behind: the version only
    // becomes visible to Roblox once every package is in place.
    auto abandon = [&](const std::string& message) {
        std::error_code ignored;
        fs::remove_all(staging, ignored);
        fail(message);
    };

    const std::vector<PackageJob> jobs = buildJobs(packages, config_.app, staging);
    if (jobs.empty()) {
        abandon("Roblox's package list had nothing TuxBlox knows how to install.");
        return;
    }

    // Downloads run several at a time: one connection spends most of its
    // life waiting, so this is where nearly all of the install time goes.
    const uint64_t totalBytes = totalPackedBytes(jobs);
    std::vector<std::atomic<uint64_t>> received(jobs.size());
    for (auto& value : received) value = 0;

    std::atomic<size_t> nextJob{0};
    std::atomic<bool> stop{false};
    std::mutex failureMutex;
    std::string failure;

    auto downloadWorker = [&]() {
        for (;;) {
            const size_t index = nextJob++;
            if (index >= jobs.size() || stop || cancelled_) return;
            const PackageJob& job = jobs[index];

            {
                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_.status = "Downloading " + job.name;
            }

            DownloadOutcome outcome{DownloadResult::Failed, "no mirror attempted"};
            for (const auto& mirror : mirrors) {
                outcome = downloadFile(
                    setupCdnUrl(mirror, config_.channel, hash, job.name), job.archivePath,
                    [&](uint64_t now, uint64_t) {
                        received[index] = now;
                        uint64_t done = 0;
                        for (const auto& value : received) done += value;
                        const double fraction =
                            totalBytes > 0 ? static_cast<double>(done) / totalBytes : 0.0;
                        std::lock_guard<std::mutex> lock(mutex_);
                        snapshot_.overallPercent = 6.0 + 64.0 * fraction;
                    },
                    &cancelled_);
                if (outcome.result == DownloadResult::Ok) break;
                if (stop || cancelled_) return;
            }

            if (outcome.result != DownloadResult::Ok) {
                std::lock_guard<std::mutex> lock(failureMutex);
                if (failure.empty()) {
                    failure = "Could not download " + job.name + ": " + outcome.errorMessage;
                }
                stop = true;
                return;
            }
            if (md5File(job.archivePath) != job.md5) {
                std::lock_guard<std::mutex> lock(failureMutex);
                if (failure.empty()) {
                    failure = "Checksum mismatch for " + job.name +
                              " -- refusing to install a corrupted package.";
                }
                stop = true;
                return;
            }
            received[index] = job.packedSize;
        }
    };

    {
        const size_t workers = std::min<size_t>(kDownloadWorkers, jobs.size());
        std::vector<std::thread> pool;
        pool.reserve(workers);
        for (size_t i = 0; i < workers; ++i) pool.emplace_back(downloadWorker);
        for (auto& worker : pool) worker.join();
    }

    if (!failure.empty()) { abandon(failure); return; }
    if (cancelled_) { abandon("Cancelled."); return; }

    // Extraction stays one at a time: several packages unpack into the same
    // folders, and libarchive writing them at once would race on the
    // directories it creates.
    for (size_t i = 0; i < jobs.size(); ++i) {
        if (cancelled_) { abandon("Cancelled."); return; }
        const PackageJob& job = jobs[i];
        const double base = 70.0 + 28.0 * (static_cast<double>(i) / jobs.size());

        setStatus("Extracting " + job.name, base);
        try {
            extractZip(job.archivePath, job.destDir);
        } catch (const std::exception& e) {
            abandon(std::string("Could not extract ") + job.name + ": " + e.what());
            return;
        }

        setStatus("Installing " + job.name, base + 28.0 / jobs.size() * 0.5);
        fs::remove(job.archivePath, error);
    }

    {
        std::ofstream appSettings(staging + "/AppSettings.xml", std::ios::binary);
        appSettings << appSettingsXml();
        if (!appSettings) {
            abandon("Could not write AppSettings.xml into " + staging);
            return;
        }
    }

    // One rename is what makes the version appear, so a half-written install
    // is never visible under its real name.
    fs::rename(staging, target, error);
    if (error) {
        abandon("Could not move the finished version into place: " + error.message());
        return;
    }

    finish(updating ? "Roblox is up to date" : "Installed " + hash);
}

} // namespace tuxblox
