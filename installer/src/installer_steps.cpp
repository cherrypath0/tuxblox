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
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string resolveInstallDir(const std::string& override) {
    return override.empty() ? installDir() : override;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Whether this artifact needs to be extracted rather than just moved into
// place -- inferred from its own download URL rather than which component
// key it happens to be under, so any future archive-shaped artifact is
// handled the same way without a code change here.
//
// PACKING CONVENTION for any archive artifact: it must be packed WITHOUT a
// top-level directory matching its own filename -- entries should extract
// directly to e.g. dist/proton, runtime/, etc. (`tar --zstd -cf
// whatever.tar.zst -C <build-output-dir> .`, not one level above it). A
// tarball packed the wrong way silently produces
// <filename>/<filename>/... and breaks anything that expects the
// extracted contents directly under the target directory (e.g. Proton's
// ProtonBuild/dist/proton).
bool isArchiveUrl(const std::string& url) {
    static const std::vector<std::string> kArchiveExtensions = {".tar.zst", ".tar.gz", ".tar.xz", ".tar.bz2"};
    for (const auto& ext : kArchiveExtensions) {
        if (endsWith(url, ext)) return true;
    }
    return false;
}

// installDir + artifact.path + "/" + artifact.filename -- artifact.path is
// "/" (installDir itself) or "/somefolder" (installDir/somefolder), per
// the manifest schema.
std::string resolveTargetPath(const std::string& installDir, const Artifact& art) {
    std::string p = art.path;
    if (!p.empty() && p.front() != '/') p = "/" + p;
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    if (p.empty() || p == "/") return installDir + "/" + art.filename;
    return installDir + p + "/" + art.filename;
}

struct ArtifactPlan {
    std::string name; // manifest component key, e.g. "proton"
    Artifact artifact;
    bool isArchive = false;
    std::string tmpPath;
    std::string targetPath; // final file path, or extraction directory if isArchive
};

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

    // A plan per artifact, built up front -- lets total download bytes (and
    // therefore each phase's progress weight) be known before any network
    // activity starts, and lets a missing "launcher" artifact fail fast
    // instead of only after downloading everything else.
    std::vector<ArtifactPlan> plans;
    plans.reserve(manifest.artifacts.size());
    uint64_t totalArtifactBytes = 0;
    bool hasLauncher = false;
    for (const auto& [name, art] : manifest.artifacts) {
        ArtifactPlan p;
        p.name = name;
        p.artifact = art;
        p.isArchive = isArchiveUrl(art.url);
        p.targetPath = resolveTargetPath(dir, art);
        p.tmpPath = dir + "/." + name + (p.isArchive ? ".tar.part" : ".part");
        totalArtifactBytes += art.sizeBytes;
        if (name == "launcher") hasLauncher = true;
        plans.push_back(std::move(p));
    }
    if (plans.empty()) {
        return {false, false, "manifest has no artifacts to install", ""};
    }
    if (!hasLauncher) {
        return {false, false, "manifest has no 'launcher' artifact -- nothing to hand off to", ""};
    }

    // Phase budget: a small fixed slice for creating the directory and each
    // Roblox pre-warm step, the rest split across artifacts proportional to
    // their declared size (falls back to an even split if sizes are all
    // zero, e.g. a minimal test fixture).
    constexpr double kCreateDirWeight = 2.0;
    constexpr double kRobloxWeight = 5.0;
    const double artifactBudget = 100.0 - kCreateDirWeight - 2 * kRobloxWeight;

    std::vector<ProgressPhase> phases;
    phases.push_back({isUpgrade ? "Preparing" : "Creating TuxBlox directory", kCreateDirWeight});
    for (const auto& plan : plans) {
        double weight = totalArtifactBytes > 0
            ? artifactBudget * (static_cast<double>(plan.artifact.sizeBytes) / static_cast<double>(totalArtifactBytes))
            : artifactBudget / static_cast<double>(plans.size());
        std::string label = (isUpgrade ? "Upgrading " : "Downloading ") + plan.artifact.displayname;
        phases.push_back({label, weight});
    }
    const size_t robloxPlayerPhase = phases.size();
    phases.push_back({"Downloading Roblox Player", kRobloxWeight});
    const size_t robloxStudioPhase = phases.size();
    phases.push_back({"Downloading Roblox Studio", kRobloxWeight});

    Progress progress(phases);
    auto report = [&](size_t phaseIndex, double fraction) {
        progress.beginPhase(phaseIndex);
        progress.setPhaseFraction(fraction);
        if (onProgress) onProgress(progress.currentLabel(), progress.overallPercent());
    };
    auto isCancelled = [&]() { return cancel && cancel->load(); };

    std::vector<std::string> tmpPathsInFlight;
    auto cleanupTmpPaths = [&]() {
        // fs::remove() is safe on a nonexistent path -- returns false
        // rather than throwing -- so unconditionally sweeping every temp
        // path seen so far is safe regardless of which one is actually
        // still there.
        for (const auto& p : tmpPathsInFlight) fs::remove(p);
    };

    try {
        report(0, 0.0);
        if (isCancelled()) return {false, true, "", ""};
        fs::create_directories(dir + "/steamapps");
        fs::create_directories(dir + "/runtime");
        report(0, 1.0);

        std::string launcherFinalPath;

        for (size_t i = 0; i < plans.size(); ++i) {
            const auto& plan = plans[i];
            const size_t phaseIndex = i + 1;

            report(phaseIndex, 0.0);
            tmpPathsInFlight.push_back(plan.tmpPath);
            auto dlOutcome = downloadFile(plan.artifact.url, plan.tmpPath,
                [&](uint64_t now, uint64_t total) {
                    // Some CDN configurations (chunked responses) don't
                    // report a Content-Length, leaving curl's `total` at 0
                    // for the whole transfer -- fall back to the
                    // manifest's declared size so the bar still advances
                    // instead of sitting at 0% until it jumps to 100%.
                    report(phaseIndex, downloadProgressFraction(now, total, plan.artifact.sizeBytes) * 0.9);
                }, cancel);
            if (dlOutcome.result == DownloadResult::Cancelled) { cleanupTmpPaths(); return {false, true, "", ""}; }
            if (dlOutcome.result == DownloadResult::Failed) {
                cleanupTmpPaths();
                return {false, false, "Downloading " + plan.artifact.displayname + " failed: " + dlOutcome.errorMessage, ""};
            }

            std::string actualSha = sha256File(plan.tmpPath);
            if (actualSha != plan.artifact.sha256) {
                cleanupTmpPaths();
                return {false, false,
                        plan.artifact.displayname + " download checksum mismatch (expected " +
                        plan.artifact.sha256 + ", got " + actualSha + ")", ""};
            }
            report(phaseIndex, 0.95);
            if (isCancelled()) { cleanupTmpPaths(); return {false, true, "", ""}; }

            if (plan.isArchive) {
                // Only wipe the extraction target now that the replacement
                // has been downloaded *and* checksum-verified -- and only
                // that artifact's own directory, never anything else under
                // installDir (runtime/, steamapps/, other artifacts, etc.).
                if (isUpgrade) {
                    fs::remove_all(plan.targetPath);
                }
                extractTarZst(plan.tmpPath, plan.targetPath,
                    [&](uint64_t now, uint64_t total) {
                        double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
                        report(phaseIndex, 0.95 + 0.05 * frac);
                    });
                fs::remove(plan.tmpPath);
            } else {
                fs::create_directories(fs::path(plan.targetPath).parent_path());
                std::error_code renameEc;
                fs::rename(plan.tmpPath, plan.targetPath, renameEc);
                if (renameEc) {
                    cleanupTmpPaths();
                    return {false, false, "Failed to install " + plan.artifact.displayname + ": " + renameEc.message(), ""};
                }
                chmod(plan.targetPath.c_str(), 0755);
            }
            report(phaseIndex, 1.0);
            if (isCancelled()) return {false, true, "", ""};

            if (plan.name == "launcher") {
                launcherFinalPath = plan.targetPath;
            }
        }

        // Roblox Player/Studio pre-warm -- unrelated to manifest artifacts,
        // unchanged from before this became generic.
        report(robloxPlayerPhase, 0.0);
        if (isCancelled()) return {false, true, "", ""};
        {
            const std::string playerDir = dir + "/RobloxPlayer";
            const std::string playerInstallerPath = playerDir + "/RobloxPlayerInstaller.exe";
            if (!fs::exists(playerInstallerPath)) {
                fs::create_directories(playerDir);
                // No manifest-declared size for these (fetched from Roblox's own CDN, outside the Manifest) -- no fallback available.
                auto robloxOutcome = downloadFile(robloxPlayerInstallerUrl, playerInstallerPath,
                    [&](uint64_t now, uint64_t total) {
                        double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
                        report(robloxPlayerPhase, frac);
                    }, cancel);
                if (robloxOutcome.result == DownloadResult::Cancelled) return {false, true, "", ""};
                // DownloadResult::Failed is intentionally ignored -- the
                // launcher's own lazy-download fallback
                // (resolveOrBootstrapExePath) covers a missing cache entry
                // when the user actually clicks Launch.
            }
        }
        report(robloxPlayerPhase, 1.0);

        report(robloxStudioPhase, 0.0);
        if (isCancelled()) return {false, true, "", ""};
        {
            const std::string studioDir = dir + "/RobloxStudio";
            const std::string studioInstallerPath = studioDir + "/RobloxStudioInstaller.exe";
            if (!fs::exists(studioInstallerPath)) {
                fs::create_directories(studioDir);
                auto robloxOutcome = downloadFile(robloxStudioInstallerUrl, studioInstallerPath,
                    [&](uint64_t now, uint64_t total) {
                        double frac = total > 0 ? static_cast<double>(now) / static_cast<double>(total) : 0.0;
                        report(robloxStudioPhase, frac);
                    }, cancel);
                if (robloxOutcome.result == DownloadResult::Cancelled) return {false, true, "", ""};
            }
        }
        report(robloxStudioPhase, 1.0);

        return {true, false, "", launcherFinalPath};
    } catch (const std::exception& e) {
        cleanupTmpPaths();
        return {false, false, e.what(), ""};
    }
}

} // namespace tuxblox
