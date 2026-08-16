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

#include "updater.h"
#include "checksum.h"
#include "downloader.h"
#include "install_paths.h"
#include "manifest.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <sys/stat.h>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string resolveInstallDir(const std::string& override) {
    return override.empty() ? installDir() : override;
}

// Splits a dot-separated numeric version ("1.0.0") into its components for
// ordered comparison. atoi(), not stoi(), so a non-numeric component reads
// as 0 instead of throwing -- this is fed by a remote-controlled manifest
// field, not one this codebase's own build produces malformed.
std::vector<int> parseVersionParts(const std::string& version) {
    std::vector<int> parts;
    std::stringstream ss(version);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(std::atoi(part.c_str()));
    }
    return parts;
}

} // namespace

bool versionNeedsUpdate(const std::string& installed, const std::string& required) {
    std::vector<int> installedParts = parseVersionParts(installed);
    std::vector<int> requiredParts = parseVersionParts(required);
    size_t count = std::max(installedParts.size(), requiredParts.size());
    for (size_t i = 0; i < count; i++) {
        int requiredPart = i < requiredParts.size() ? requiredParts[i] : 0;
        int installedPart = i < installedParts.size() ? installedParts[i] : 0;
        if (requiredPart != installedPart) return requiredPart > installedPart;
    }
    return false;
}

double downloadProgressFraction(uint64_t now, uint64_t total, uint64_t manifestSize) {
    uint64_t effectiveTotal = total > 0 ? total : manifestSize;
    return effectiveTotal > 0 ? static_cast<double>(now) / static_cast<double>(effectiveTotal) : 0.0;
}

EnsureInstallerResult ensureInstallerBinary(const Manifest& manifest, const std::string& dir,
                                             const std::atomic<bool>* cancel,
                                             const UpdateProgressFn& onProgress) {
    auto report = [&](UpdatePhase phase, double fraction, const std::string& err = "") {
        if (onProgress) onProgress({phase, fraction, err});
    };

    report(UpdatePhase::PreparingUpdater, 0.0);
    const std::string installerPath = dir + "/TuxBloxInstaller";

    // Re-fetch the installer only if it's missing or stale relative to the
    // manifest -- a bare "is it there" check isn't enough, since the
    // installer artifact itself can change independently of requiredVersion
    // (a re-upload of the same release's installer binary, for instance).
    bool needsFetch = true;
    if (fs::exists(installerPath)) {
        try {
            needsFetch = (sha256File(installerPath) != manifest.installer.sha256);
        } catch (const std::exception&) {
            needsFetch = true; // unreadable/corrupt -- treat as missing
        }
    }

    if (needsFetch) {
        const std::string newPath = installerPath + ".new";
        auto dlOutcome = downloadFile(manifest.installer.url, newPath,
            [&](uint64_t now, uint64_t total) {
                // Some CDN configurations (chunked responses) don't report a
                // Content-Length, leaving curl's `total` at 0 for the whole
                // transfer -- fall back to the manifest's declared size
                // (fetched and verified before this step runs) so the bar
                // still advances instead of sitting at 0% until it jumps to
                // 100%.
                report(UpdatePhase::PreparingUpdater, downloadProgressFraction(now, total, manifest.installer.sizeBytes));
            }, cancel);

        if (dlOutcome.result == DownloadResult::Cancelled) {
            report(UpdatePhase::Idle, 0.0);
            return {};
        }
        if (dlOutcome.result == DownloadResult::Failed) {
            std::string err = "Downloading updater failed: " + dlOutcome.errorMessage;
            report(UpdatePhase::Error, 0.0, err);
            return {false, "", err};
        }

        std::string actualSha = sha256File(newPath);
        if (actualSha != manifest.installer.sha256) {
            fs::remove(newPath);
            report(UpdatePhase::Error, 0.0, "Updater checksum mismatch");
            return {false, "", "Updater checksum mismatch"};
        }

        chmod(newPath.c_str(), 0755);
        std::error_code renameEc;
        fs::rename(newPath, installerPath, renameEc);
        if (renameEc) {
            fs::remove(newPath);
            std::string err = "Failed to install updater: " + renameEc.message();
            report(UpdatePhase::Error, 0.0, err);
            return {false, "", err};
        }
    } else {
        chmod(installerPath.c_str(), 0755);
    }

    return {true, installerPath, ""};
}

UpdateResult runUpdateCheck(const std::string& currentLauncherVersion,
                             const std::string& baseUrl,
                             const std::string& channel,
                             const std::string& requiredVersion,
                             const UpdateProgressFn& onProgress,
                             const std::atomic<bool>* cancel,
                             const std::string& installDirOverride) {
    auto report = [&](UpdatePhase phase, double fraction, const std::string& err = "") {
        if (onProgress) onProgress({phase, fraction, err});
    };

    report(UpdatePhase::CheckingManifest, 0.0);

    const std::string manifestUrl = baseUrl + "/v1/" + channel + "/" + requiredVersion + "/manifest.json";

    Manifest manifest;
    try {
        std::string json = fetchManifestJson(manifestUrl, cancel);
        manifest = parseManifest(json, baseUrl);
    } catch (const std::exception& e) {
        report(UpdatePhase::Error, 0.0, e.what());
        return {};
    }

    const std::string dir = resolveInstallDir(installDirOverride);

    // One release version now covers both the launcher and Proton together
    // (see updater.h's doc comment) -- both compared against the same
    // requiredVersion rather than separate tuxbloxVersion/protonVersion
    // fields that no longer exist on Manifest.
    const bool launcherNeedsUpdate = versionNeedsUpdate(currentLauncherVersion, requiredVersion);
    auto installedProtonVersion = readInstalledProtonVersion(dir);
    const bool protonNeedsUpdate =
        !installedProtonVersion.has_value() || versionNeedsUpdate(*installedProtonVersion, requiredVersion);

    if (!launcherNeedsUpdate && !protonNeedsUpdate) {
        report(UpdatePhase::UpToDate, 1.0);
        return {};
    }

    EnsureInstallerResult ensured = ensureInstallerBinary(manifest, dir, cancel, onProgress);
    if (!ensured.ok) return {};

    return {true, ensured.installerPath, !installedProtonVersion.has_value()};
}

} // namespace tuxblox
