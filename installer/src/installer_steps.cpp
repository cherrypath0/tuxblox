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

#include "installer_steps.h"
#include "install_paths.h"
#include "downloader.h"
#include "checksum.h"
#include "tar_extract.h"
#include <filesystem>
#include <sys/stat.h>

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

InstallOutcome runInstall(const Manifest& manifest,
                           const StepProgressFn& onProgress,
                           const std::atomic<bool>* cancel,
                           bool isUpgrade,
                           const std::string& installDirOverride,
                           const std::string& robloxPlayerInstallerUrl,
                           const std::string& robloxStudioInstallerUrl) {
    const std::string dir = resolveInstallDir(installDirOverride);
    Progress progress;

    auto report = [&](Step step, double fraction) {
        progress.beginStep(step);
        progress.setStepFraction(fraction);
        if (onProgress) onProgress(step, progress.overallPercent());
    };

    auto isCancelled = [&]() { return cancel && cancel->load(); };

    // Declared here (rather than inside the try block) so the catch handler
    // below can safely attempt cleanup regardless of which step threw.
    const std::string tarPath = dir + "/.protonbuild.tar.zst.part";
    const std::string launcherTmpPath = dir + "/.TuxBloxLauncher.part";
    const std::string installerTmpPath = dir + "/.TuxBloxInstaller.part";

    try {
        // Step 1: Creating TuxBlox directory (idempotent on an upgrade --
        // create_directories() is a no-op if these already exist).
        report(Step::CreatingDirectory, 0.0);
        if (isCancelled()) return {false, true, ""};
        fs::create_directories(dir + "/steamapps");
        fs::create_directories(dir + "/runtime");
        report(Step::CreatingDirectory, 1.0);

        // Step 2: Downloading Proton
        report(Step::DownloadingProton, 0.0);
        auto dlOutcome = downloadFile(manifest.protonbuild.url, tarPath,
            [&](uint64_t now, uint64_t total) {
                // Some CDN configurations (chunked responses) don't report a
                // Content-Length, leaving curl's `total` at 0 for the whole
                // transfer -- fall back to the manifest's declared size
                // (fetched and verified before this step runs) so the bar
                // still advances instead of sitting at 0% until it jumps to
                // 100%.
                report(Step::DownloadingProton, downloadProgressFraction(now, total, manifest.protonbuild.sizeBytes));
            }, cancel);
        if (dlOutcome.result == DownloadResult::Cancelled) return {false, true, ""};
        if (dlOutcome.result == DownloadResult::Failed) {
            return {false, false, "Downloading Proton failed: " + dlOutcome.errorMessage};
        }
        report(Step::DownloadingProton, 1.0);

        // Step 3: Verifying Proton
        report(Step::VerifyingProton, 0.0);
        std::string actualSha = sha256File(tarPath);
        if (actualSha != manifest.protonbuild.sha256) {
            fs::remove(tarPath);
            return {false, false,
                    "Proton download checksum mismatch (expected " +
                    manifest.protonbuild.sha256 + ", got " + actualSha + ")"};
        }
        report(Step::VerifyingProton, 1.0);
        if (isCancelled()) { fs::remove(tarPath); return {false, true, ""}; }

        // Step 4: Extracting Proton
        //
        // PACKING CONVENTION: protonbuild-*.tar.zst must be packed WITHOUT a
        // top-level "ProtonBuild/" directory -- entries should extract directly
        // to e.g. dist/proton, runtime/, etc. Pack from inside the build-output
        // directory (`tar --zstd -cf protonbuild-x.y.z.tar.zst -C <build-output-dir> .`)
        // rather than one level above it. A tarball packed the wrong way
        // silently produces ProtonBuild/ProtonBuild/... and breaks launch.sh,
        // which looks for ProtonBuild/dist/proton.
        report(Step::ExtractingProton, 0.0);
        // On an upgrade, ProtonBuild/ already exists from the previous
        // install -- wipe it first (only now that the replacement has been
        // downloaded *and* checksum-verified above) so a build that removed
        // files doesn't leave stale ones behind alongside the new build.
        // Never touches anything outside ProtonBuild/ (runtime/, steamapps/,
        // etc. are untouched either way).
        if (isUpgrade) {
            fs::remove_all(dir + "/ProtonBuild");
        }
        extractTarZst(tarPath, dir + "/ProtonBuild",
            [&](uint64_t now, uint64_t total) {
                double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
                report(Step::ExtractingProton, frac);
            });
        fs::remove(tarPath);
        report(Step::ExtractingProton, 1.0);
        if (isCancelled()) return {false, true, ""};

        // Step 5: Downloading Launcher
        report(Step::DownloadingLauncher, 0.0);
        dlOutcome = downloadFile(manifest.launcher.url, launcherTmpPath,
            [&](uint64_t now, uint64_t total) {
                report(Step::DownloadingLauncher, downloadProgressFraction(now, total, manifest.launcher.sizeBytes));
            }, cancel);
        if (dlOutcome.result == DownloadResult::Cancelled) return {false, true, ""};
        if (dlOutcome.result == DownloadResult::Failed) {
            return {false, false, "Downloading Launcher failed: " + dlOutcome.errorMessage};
        }
        report(Step::DownloadingLauncher, 1.0);

        // Step 6: Verifying Launcher
        report(Step::VerifyingLauncher, 0.0);
        actualSha = sha256File(launcherTmpPath);
        if (actualSha != manifest.launcher.sha256) {
            fs::remove(launcherTmpPath);
            return {false, false,
                    "Launcher download checksum mismatch (expected " +
                    manifest.launcher.sha256 + ", got " + actualSha + ")"};
        }
        report(Step::VerifyingLauncher, 1.0);
        if (isCancelled()) { fs::remove(launcherTmpPath); return {false, true, ""}; }

        // Step 7: Downloading Installer (persisted alongside the launcher so
        // the launcher can invoke this same installer again later to apply
        // future updates, without needing its own download/extract logic).
        report(Step::DownloadingInstaller, 0.0);
        dlOutcome = downloadFile(manifest.installer.url, installerTmpPath,
            [&](uint64_t now, uint64_t total) {
                report(Step::DownloadingInstaller, downloadProgressFraction(now, total, manifest.installer.sizeBytes));
            }, cancel);
        if (dlOutcome.result == DownloadResult::Cancelled) { fs::remove(launcherTmpPath); return {false, true, ""}; }
        if (dlOutcome.result == DownloadResult::Failed) {
            fs::remove(launcherTmpPath);
            return {false, false, "Downloading Installer failed: " + dlOutcome.errorMessage};
        }
        report(Step::DownloadingInstaller, 1.0);

        // Step 8: Verifying Installer
        report(Step::VerifyingInstaller, 0.0);
        actualSha = sha256File(installerTmpPath);
        if (actualSha != manifest.installer.sha256) {
            fs::remove(launcherTmpPath);
            fs::remove(installerTmpPath);
            return {false, false,
                    "Installer download checksum mismatch (expected " +
                    manifest.installer.sha256 + ", got " + actualSha + ")"};
        }
        report(Step::VerifyingInstaller, 1.0);
        if (isCancelled()) {
            fs::remove(launcherTmpPath);
            fs::remove(installerTmpPath);
            return {false, true, ""};
        }

        // Step 9: Downloading the Roblox Player installer (best-effort cache
        // pre-warm -- see the "isCancelled" note above stepLabel's comment
        // in installer_steps.h for why a download *failure* here doesn't
        // fail the whole install, but an explicit user cancel still does).
        report(Step::DownloadingRobloxPlayer, 0.0);
        if (isCancelled()) return {false, true, ""};
        {
            const std::string playerDir = dir + "/RobloxPlayer";
            const std::string playerInstallerPath = playerDir + "/RobloxPlayerInstaller.exe";
            if (!fs::exists(playerInstallerPath)) {
                fs::create_directories(playerDir);
                auto robloxOutcome = downloadFile(robloxPlayerInstallerUrl, playerInstallerPath,
                    [&](uint64_t now, uint64_t total) {
                        double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
                        report(Step::DownloadingRobloxPlayer, frac);
                    }, cancel);
                if (robloxOutcome.result == DownloadResult::Cancelled) return {false, true, ""};
                // DownloadResult::Failed is intentionally ignored -- the
                // launcher's own lazy-download fallback
                // (resolveOrBootstrapExePath) covers a missing cache entry
                // when the user actually clicks Launch.
            }
        }
        report(Step::DownloadingRobloxPlayer, 1.0);

        // Step 10: Downloading the Roblox Studio installer (same rationale).
        report(Step::DownloadingRobloxStudio, 0.0);
        if (isCancelled()) return {false, true, ""};
        {
            const std::string studioDir = dir + "/RobloxStudio";
            const std::string studioInstallerPath = studioDir + "/RobloxStudioInstaller.exe";
            if (!fs::exists(studioInstallerPath)) {
                fs::create_directories(studioDir);
                auto robloxOutcome = downloadFile(robloxStudioInstallerUrl, studioInstallerPath,
                    [&](uint64_t now, uint64_t total) {
                        double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
                        report(Step::DownloadingRobloxStudio, frac);
                    }, cancel);
                if (robloxOutcome.result == DownloadResult::Cancelled) return {false, true, ""};
            }
        }
        report(Step::DownloadingRobloxStudio, 1.0);

        // Step 11: Moving Executables
        report(Step::MovingExecutables, 0.0);
        const std::string launcherFinalPath = dir + "/TuxBloxLauncher";
        fs::rename(launcherTmpPath, launcherFinalPath);
        chmod(launcherFinalPath.c_str(), 0755);
        const std::string installerFinalPath = dir + "/TuxBloxInstaller";
        fs::rename(installerTmpPath, installerFinalPath);
        chmod(installerFinalPath.c_str(), 0755);
        report(Step::MovingExecutables, 1.0);

        return {true, false, ""};
    } catch (const std::exception& e) {
        // Clean up any partial download left behind by whichever step threw.
        // fs::remove() is safe to call on a nonexistent path -- it just
        // returns false rather than throwing -- so trying all three
        // unconditionally is safe regardless of which step failed.
        fs::remove(tarPath);
        fs::remove(launcherTmpPath);
        fs::remove(installerTmpPath);
        return {false, false, e.what()};
    }
}

} // namespace tuxblox
