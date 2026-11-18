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
#include "i18n.h"
#include "checksum.h"
#include "tar_extract.h"
#include "downloader.h"
#include "install_paths.h"
#include "manifest.h"
#include "release_file.h"
#include "update_decision.h"
#include "release_stamp.h"
#include "version.h"
#include "settings.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

EnsureCompatResult ensureCompatInstalled(const ReleaseFile& release, const std::string& baseUrl,
                                          const std::string& dir,
                                          const std::atomic<bool>* cancel,
                                          const UpdateProgressFn& onProgress) {
    auto report = [&](UpdatePhase phase, double fraction, const std::string& err = "") {
        if (onProgress) onProgress({phase, fraction, err});
    };

    const auto entry = release.components.find("compat");
    if (entry == release.components.end() || !entry->second.x86_64.has_value()) {
        const std::string err =
            i18n::trf("Release {0} publishes no compatibility layer.", {release.release});
        report(UpdatePhase::Error, 0.0, err);
        return {false, err};
    }
    const ComponentBytes& wanted = *entry->second.x86_64;

    // Either answer means the copy on disk is the one this release names, and re-fetching it would
    // cost a gigabyte for nothing. A stamped layer says which release it was published in; one from
    // a release published before stamping says its own component version instead, and older layers
    // answer --component-version with nothing at all, so the identity is the only thing to go on.
    // Comparing against the release alone made every launch re-download the whole layer.
    const std::string have = readComponentIdentity(dir, "compat").value_or("");
    if (!have.empty() && (have == release.release || have == entry->second.version)) {
        return {true, ""};
    }

    report(UpdatePhase::FetchingCompat, 0.0);
    const std::string target = dir + "/compat";
    const std::string archive = target + ".tar.zst.incoming";

    auto download = [&](const std::string& from) {
        return downloadFile(from, archive, [&](uint64_t now, uint64_t total) {
            // The unpack is the last tenth, so the bar keeps moving through it.
            report(UpdatePhase::FetchingCompat,
                   0.9 * downloadProgressFraction(now, total, wanted.size));
        }, cancel);
    };

    DownloadOutcome outcome;
    for (const std::string& from : componentUrls(componentBaseUrls(baseUrl), release, "compat", hostArch())) {
        outcome = download(from);
        if (outcome.result != DownloadResult::Failed) break;
    }
    if (outcome.result == DownloadResult::Cancelled) {
        std::error_code ignored;
        fs::remove(archive, ignored);
        report(UpdatePhase::Idle, 0.0);
        return {false, ""};
    }
    if (outcome.result == DownloadResult::Failed) {
        std::error_code ignored;
        fs::remove(archive, ignored);
        const std::string err =
            i18n::trf("Downloading the compatibility layer failed: {0}", {outcome.errorMessage});
        report(UpdatePhase::Error, 0.0, err);
        return {false, err};
    }

    std::string actual;
    try {
        actual = sha256File(archive);
    } catch (const std::exception&) {
        actual.clear();
    }
    if (actual != wanted.sha256) {
        std::error_code ignored;
        fs::remove(archive, ignored);
        const std::string err = i18n::tr("The downloaded compatibility layer did not match what the "
                                          "release says it should be, so it was discarded.");
        report(UpdatePhase::Error, 0.0, err);
        return {false, err};
    }

    // Unpacked beside the old copy and swapped in, so a crash leaves either the old layer or none,
    // never half of one. Nothing else under the install root is touched.
    const std::string staging = target + ".unpacking";
    std::error_code ec;
    fs::remove_all(staging, ec);
    try {
        extractTarZst(archive, staging, [&](uint64_t now, uint64_t total) {
            const double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
            report(UpdatePhase::FetchingCompat, 0.9 + 0.1 * frac);
        });
    } catch (const std::exception& e) {
        fs::remove_all(staging, ec);
        fs::remove(archive, ec);
        const std::string err = i18n::trf("Unpacking the compatibility layer failed: {0}", {e.what()});
        report(UpdatePhase::Error, 0.0, err);
        return {false, err};
    }
    fs::remove(archive, ec);

    fs::remove_all(target, ec);
    fs::rename(staging, target, ec);
    if (ec) {
        fs::remove_all(staging, ec);
        const std::string err = i18n::trf("Putting the compatibility layer in place failed: {0}",
                                           {ec.message()});
        report(UpdatePhase::Error, 0.0, err);
        return {false, err};
    }

    report(UpdatePhase::UpToDate, 1.0);
    return {true, ""};
}

bool flatpakBundleBehind(const ReleaseFile& release, const std::string& installDir) {
    // compat is not here on purpose: it is the one part a Flatpak fetches for itself.
    for (const char* name : {"launcher", "libtuxblox", "studio-mcp"}) {
        const auto entry = release.components.find(name);
        if (entry == release.components.end()) continue;
        const std::optional<std::string> have = readComponentVersion(installDir, name);
        if (!have.has_value()) continue;
        if (compareDottedVersions(*have, entry->second.version) < 0) return true;
    }
    return false;
}

std::string flatpakLayerRelease(bool bundleBehind, const std::string& ownRelease,
                                 const std::string& channelRelease) {
    return bundleBehind && !ownRelease.empty() ? ownRelease : channelRelease;
}

bool flatpakLayerResolved(const ReleaseFile& answered, const std::string& wanted) {
    return !answered.release.empty() && answered.release == wanted;
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
        const std::string err = i18n::trf("Release {0} publishes no updater to hand over to.", {release.release});
        report(UpdatePhase::Error, 0.0, err);
        return {false, "", err};
    }
    const ComponentBytes& wanted = *entry->second.x86_64;

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
        auto download = [&](const std::string& from) {
            return downloadFile(from, newPath, [&](uint64_t now, uint64_t total) {
                // Some CDN configurations (chunked responses) don't report a
                // Content-Length, leaving curl's `total` at 0 for the whole
                // transfer -- fall back to the manifest's declared size
                // (fetched and verified before this step runs) so the bar
                // still advances instead of sitting at 0% until it jumps to
                // 100%.
                report(UpdatePhase::PreparingUpdater, downloadProgressFraction(now, total, wanted.size));
            }, cancel);
        };

        DownloadOutcome dlOutcome;
        for (const std::string& from : componentUrls(componentBaseUrls(baseUrl), release, "installer", hostArch())) {
            dlOutcome = download(from);
            if (dlOutcome.result != DownloadResult::Failed) break;
        }

        if (dlOutcome.result == DownloadResult::Cancelled) {
            report(UpdatePhase::Idle, 0.0);
            return {};
        }
        if (dlOutcome.result == DownloadResult::Failed) {
            std::string err = i18n::trf("Downloading updater failed: {0}", {dlOutcome.errorMessage});
            report(UpdatePhase::Error, 0.0, err);
            return {false, "", err};
        }

        std::string actualSha = sha256File(newPath);
        if (actualSha != wanted.sha256) {
            fs::remove(newPath);
            const std::string err = i18n::tr("Updater checksum mismatch");
            report(UpdatePhase::Error, 0.0, err);
            return {false, "", err};
        }

        chmod(newPath.c_str(), 0755);
        std::error_code renameEc;
        fs::rename(newPath, installerPath, renameEc);
        if (renameEc) {
            fs::remove(newPath);
            std::string err = i18n::trf("Failed to install updater: {0}", {renameEc.message()});
            report(UpdatePhase::Error, 0.0, err);
            return {false, "", err};
        }
    } else {
        chmod(installerPath.c_str(), 0755);
    }

    return {true, installerPath, ""};
}

std::vector<ComponentVersion> readComponentVersions(const std::string& installDir) {
    std::vector<ComponentVersion> components;
    for (const std::string& name : componentNames()) {
        // The launcher is the one asking; runUpdateCheck() supplies its entry, because which
        // binary answers for it depends on which install is being judged.
        if (name == "launcher") continue;
        components.push_back({name, componentLayout(name)->displayName, "",
                               readComponentIdentity(installDir, name).value_or("")});
    }
    return components;
}

std::string writeReleaseHandoff(const std::string& installDir, const std::string& releaseJson) {
    const std::string path = installDir + "/.release-incoming.json";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return i18n::tr("TuxBlox could not write to its own folder");
    out << releaseJson;
    out.flush();
    if (!out) return i18n::tr("writing the update details failed, which usually means the disk is full");
    return "";
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
        try {
            result.releaseJson = fetchManifestJson(releaseFileUrl(baseUrl, channel, namedRelease), cancel);
        } catch (const std::exception&) {
            // Including a 404: the edge not having a release file is not evidence the release does
            // not exist, only that the edge could not produce it.
            if (baseUrl != releasesEdgeUrl()) throw;
            fprintf(stderr, "TuxBlox: %s did not serve release %s; asking %s instead\n",
                    baseUrl.c_str(), namedRelease.c_str(), releasesOriginUrl().c_str());
            result.releaseJson = fetchManifestJson(
                releaseFileUrl(releasesOriginUrl(), channel, namedRelease), cancel);
        }
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
    const std::vector<ComponentVersion> installed = readComponentVersions(dir);
    for (const ComponentVersion& component : installed) {
        if (!component.version.empty()) state.installedIdentities[component.component] = component.version;
    }
    // The install being judged is the one at `dir`, which is not always the one running:
    // $TUXBLOX_ROOT points this launcher at another install, and judging that install by the
    // version of the binary that happens to be executing made every such launch read as
    // incoherent -- a forced repair that repaired nothing and re-exec'd a different binary, on
    // every launch. Asking the running binary is only a shortcut for the case where it IS the
    // one installed there.
    const std::string self = selfExePath();
    const bool runningFromThisInstall =
        !self.empty() && fs::path(self).parent_path() == fs::path(dir);
    const std::optional<std::string> launcherVersion =
        runningFromThisInstall ? std::optional<std::string>(reportedVersion(kTuxBloxVersion))
                                : readComponentIdentity(dir, "launcher");
    if (launcherVersion.has_value()) state.installedIdentities["launcher"] = *launcherVersion;
    // The install's own launcher is what it says it is, and every other component is checked
    // against that. There is nothing else to check against: the release lives inside the
    // components themselves rather than in a file beside them.
    state.installedRelease = launcherVersion.value_or("");
    // What each component says its own version is, against what the channel publishes for it.
    for (const std::string& name : componentNames()) {
        const std::optional<std::string> own = readComponentVersion(dir, name);
        if (own.has_value()) state.installedVersions[name] = *own;
        const auto published = result.release.components.find(name);
        if (published != result.release.components.end()) {
            state.publishedVersions[name] = published->second.version;
        }
    }

    // Nothing installed at all is a first run, not a broken install -- and it is the layer being
    // there that makes the rest of the pieces overdue, since one release places them all together.
    const bool compatMissing = state.installedIdentities.count("compat") == 0;

    const UpdateDecision decision = decideUpdate(state, namedRelease, settings.noRollback);

    // Says what it saw either way, so an install standing still on purpose does not look like one
    // that failed to notice.
    if (!decision.reason.empty()) {
        fprintf(stderr, "TuxBlox: %s\n", decision.reason.c_str());
    }

    if (!decision.update && !compatMissing) {
        report(UpdatePhase::UpToDate, 1.0);
        UpdateResult quiet;
        quiet.internal = decision.internal;
        return quiet;
    }

    result.needsHandoff = true;
    result.forced = decision.forced;
    result.internal = decision.internal;
    result.compatMissing = compatMissing;
    std::map<std::string, std::string> namedVersions;
    for (const auto& [name, component] : result.release.components) namedVersions[name] = component.version;
    // Component versions on both sides: what is changing in an update is which components moved,
    // and every one of them answers with the release otherwise, which is one number for all five.
    std::map<std::string, std::string> installedVersions;
    for (const std::string& name : componentNames()) {
        const std::optional<std::string> version = readComponentVersion(dir, name);
        if (version.has_value() && !version->empty()) installedVersions[name] = *version;
    }
    result.changes = updateChanges(installedVersions, namedVersions);
    return result;
}

} // namespace tuxblox
