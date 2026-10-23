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
#include "release_file.h"
#include "update_decision.h"
#include "version.h"
#include "settings.h"
#include <algorithm>
#include <cstdio>
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

} // namespace

double downloadProgressFraction(uint64_t now, uint64_t total, uint64_t manifestSize) {
    uint64_t effectiveTotal = total > 0 ? total : manifestSize;
    return effectiveTotal > 0 ? static_cast<double>(now) / static_cast<double>(effectiveTotal) : 0.0;
}

EnsureInstallerResult ensureInstallerBinary(const ReleaseFile& release, const std::string& baseUrl,
                                             const std::string& dir,
                                             const std::atomic<bool>* cancel,
                                             const UpdateProgressFn& onProgress) {
    auto report = [&](UpdatePhase phase, double fraction, const std::string& err = "") {
        if (onProgress) onProgress({phase, fraction, err});
    };

    const auto entry = release.components.find("installer");
    if (entry == release.components.end() || !entry->second.x86_64.has_value()) {
        const std::string err = "Release " + release.release + " publishes no updater to hand over to.";
        report(UpdatePhase::Error, 0.0, err);
        return {false, "", err};
    }
    const ComponentBytes& wanted = *entry->second.x86_64;
    const std::string installerUrl =
        componentUrl(baseUrl, "installer", entry->second.version, hostArch());

    report(UpdatePhase::PreparingUpdater, 0.0);
    const std::string installerPath = dir + "/TuxBloxInstaller";

    // Re-fetch the installer only if it's missing or stale relative to the
    // manifest -- a bare "is it there" check isn't enough, since the
    // installer artifact itself can change independently of requiredVersion
    // (a re-upload of the same release's installer binary, for instance).
    bool needsFetch = true;
    if (fs::exists(installerPath)) {
        try {
            needsFetch = (sha256File(installerPath) != wanted.sha256);
        } catch (const std::exception&) {
            needsFetch = true; // unreadable/corrupt -- treat as missing
        }
    }

    if (needsFetch) {
        const std::string newPath = installerPath + ".new";
        auto dlOutcome = downloadFile(installerUrl, newPath,
            [&](uint64_t now, uint64_t total) {
                // Some CDN configurations (chunked responses) don't report a
                // Content-Length, leaving curl's `total` at 0 for the whole
                // transfer -- fall back to the manifest's declared size
                // (fetched and verified before this step runs) so the bar
                // still advances instead of sitting at 0% until it jumps to
                // 100%.
                report(UpdatePhase::PreparingUpdater, downloadProgressFraction(now, total, wanted.size));
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
        if (actualSha != wanted.sha256) {
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

std::vector<ComponentVersion> readComponentVersions(const std::string& installDir) {
    std::vector<ComponentVersion> components = {
        {"compat",     "compatibility layer", compatDirUnder(installDir) + "/main", ""},
        {"installer",  "updater",             installDir + "/TuxBloxInstaller",     ""},
        {"studio-mcp", "Studio MCP",          installDir + "/studio-mcp",           ""},
    };
    for (ComponentVersion& component : components) {
        component.version = readBinaryVersion(component.path).value_or("");
    }
    // Read, never run: the interface stack is a set of libraries, so it ships its version as a file
    // inside its own tarball.
    components.push_back({"libtuxblox", "libraries", "",
                           readStackVersion(installDir).value_or("")});
    return components;
}

UpdateResult runUpdateCheck(const std::string& baseUrl,
                             const std::string& channel,
                             const std::string& namedRelease,
                             const UpdateProgressFn& onProgress,
                             const std::atomic<bool>* cancel,
                             const std::string& installDirOverride) {
    auto report = [&](UpdatePhase phase, double fraction, const std::string& err = "") {
        if (onProgress) onProgress({phase, fraction, err});
    };

    report(UpdatePhase::CheckingManifest, 0.0);

    UpdateResult result;
    try {
        result.releaseJson = fetchManifestJson(releaseFileUrl(baseUrl, channel, namedRelease), cancel);
        result.release = parseReleaseFile(result.releaseJson);
    } catch (const std::exception& e) {
        // A pointer naming a release whose file is not there is a broken CDN, not a reason to
        // replace a working install.
        report(UpdatePhase::Error, 0.0, e.what());
        return {};
    }

    const std::string dir = resolveInstallDir(installDirOverride);
    // Read once: two reads could disagree if the file changed in between, and this decides whether
    // the install is replaced.
    const Settings settings = loadSettings(dir);

    InstallState state;
    const RecordedRelease recorded = readRecordedRelease(dir);
    state.releaseRecorded = recorded.present;
    if (recorded.present) {
        state.recordedRelease = recorded.file.release;
        for (const auto& [name, component] : recorded.file.components) {
            state.recordedVersions[name] = component.version;
        }
    }
    const std::vector<ComponentVersion> installed = readComponentVersions(dir);
    for (const ComponentVersion& component : installed) {
        if (!component.version.empty()) state.installedVersions[component.component] = component.version;
    }
    // The launcher is the one asking, so it is not among the components read off disk -- it knows
    // its own answer, and running itself to find it out would be absurd.
    state.installedVersions["launcher"] = kTuxBloxVersion;

    // Nothing installed at all is a first run, not a broken install -- and it is the layer being
    // there that makes the rest of the pieces overdue, since one release places them all together.
    const bool compatMissing = state.installedVersions.count("compat") == 0;

    const UpdateDecision decision = decideUpdate(state, namedRelease, settings.noRollback);

    // Says what it saw either way, so an install standing still on purpose does not look like one
    // that failed to notice.
    if (!decision.reason.empty()) {
        fprintf(stderr, "TuxBlox: %s\n", decision.reason.c_str());
    }

    if (!decision.update && !compatMissing) {
        report(UpdatePhase::UpToDate, 1.0);
        return {};
    }

    result.needsHandoff = true;
    result.forced = decision.forced;
    result.compatMissing = compatMissing;
    return result;
}

} // namespace tuxblox
