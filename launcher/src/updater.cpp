#include "updater.h"
#include "checksum.h"
#include "downloader.h"
#include "install_paths.h"
#include "manifest.h"
#include <filesystem>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string resolveInstallDir(const std::string& override) {
    return override.empty() ? installDir() : override;
}

} // namespace

bool versionNeedsUpdate(const std::string& installed, const std::string& required) {
    return installed != required;
}

UpdateResult runUpdateCheck(const std::string& currentLauncherVersion,
                             const std::string& manifestUrl,
                             const UpdateProgressFn& onProgress,
                             const std::atomic<bool>* cancel,
                             const std::string& installDirOverride) {
    auto report = [&](UpdatePhase phase, double fraction, const std::string& err = "") {
        if (onProgress) onProgress({phase, fraction, err});
    };

    report(UpdatePhase::CheckingManifest, 0.0);

    Manifest manifest;
    try {
        std::string json = fetchManifestJson(manifestUrl, cancel);
        manifest = parseManifest(json);
    } catch (const std::exception& e) {
        report(UpdatePhase::Error, 0.0, e.what());
        return {};
    }

    const std::string dir = resolveInstallDir(installDirOverride);

    const bool launcherNeedsUpdate = versionNeedsUpdate(currentLauncherVersion, manifest.tuxbloxVersion);
    auto installedProtonVersion = readInstalledProtonVersion(dir);
    const bool protonNeedsUpdate =
        !installedProtonVersion.has_value() || versionNeedsUpdate(*installedProtonVersion, manifest.protonVersion);

    if (!launcherNeedsUpdate && !protonNeedsUpdate) {
        report(UpdatePhase::UpToDate, 1.0);
        return {};
    }

    report(UpdatePhase::PreparingUpdater, 0.0);
    const std::string installerPath = dir + "/TuxBloxInstaller";

    // Re-fetch the installer only if it's missing or stale relative to the
    // manifest -- a bare "is it there" check isn't enough, since the
    // installer artifact itself gets version bumps independently of
    // tuxblox_version/proton_version.
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
                double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
                report(UpdatePhase::PreparingUpdater, frac);
            }, cancel);

        if (dlOutcome.result == DownloadResult::Cancelled) {
            report(UpdatePhase::Idle, 0.0);
            return {};
        }
        if (dlOutcome.result == DownloadResult::Failed) {
            report(UpdatePhase::Error, 0.0, "Downloading updater failed: " + dlOutcome.errorMessage);
            return {};
        }

        std::string actualSha = sha256File(newPath);
        if (actualSha != manifest.installer.sha256) {
            fs::remove(newPath);
            report(UpdatePhase::Error, 0.0, "Updater checksum mismatch");
            return {};
        }

        chmod(newPath.c_str(), 0755);
        std::error_code renameEc;
        fs::rename(newPath, installerPath, renameEc);
        if (renameEc) {
            fs::remove(newPath);
            report(UpdatePhase::Error, 0.0, "Failed to install updater: " + renameEc.message());
            return {};
        }
    } else {
        chmod(installerPath.c_str(), 0755);
    }

    return {true, installerPath};
}

} // namespace tuxblox
