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

#include "install_plan.h"

#include <filesystem>

namespace tuxblox {

namespace {

const char* const kPrimaryMirror = "setup.rbxcdn.com";
const char* const kAlternateMirror = "setup-aws.rbxcdn.com";

} // namespace

std::vector<std::string> mirrorsFor(const Config& config) {
    std::vector<std::string> mirrors{config.server};
    if (config.serverPinned) return mirrors;

    if (config.server == kPrimaryMirror) {
        mirrors.push_back(kAlternateMirror);
    } else if (config.server == kAlternateMirror) {
        mirrors.push_back(kPrimaryMirror);
    }
    return mirrors;
}

std::string versionDir(const std::string& installDir, const std::string& hash) {
    std::string base = installDir;
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + "/" + hash;
}

std::string packageDestDir(const std::string& versionDir, const std::string& subdir) {
    return subdir.empty() ? versionDir : versionDir + "/" + subdir;
}

std::vector<PackageJob> buildJobs(const std::vector<PackageEntry>& packages, RobloxApp app,
                                   const std::string& stagingDir) {
    std::vector<PackageJob> jobs;
    for (const auto& package : packages) {
        const auto subdir = packageInstallSubdir(package.name, app);
        if (!subdir.has_value()) continue;
        PackageJob job;
        job.name = package.name;
        job.md5 = package.md5;
        job.packedSize = package.packedSize;
        job.archivePath = stagingDir + "/" + package.name;
        job.destDir = packageDestDir(stagingDir, *subdir);
        jobs.push_back(std::move(job));
    }
    return jobs;
}

uint64_t totalPackedBytes(const std::vector<PackageJob>& jobs) {
    uint64_t total = 0;
    for (const auto& job : jobs) total += job.packedSize;
    return total;
}

const char* versionExecutable(RobloxApp app) {
    return app == RobloxApp::Player ? "RobloxPlayerBeta.exe" : "RobloxStudioBeta.exe";
}

bool versionIsInstalled(const std::string& versionDir, RobloxApp app) {
    std::error_code error;
    return std::filesystem::exists(
        std::filesystem::path(versionDir) / versionExecutable(app), error);
}

bool versionIsInstalledForAnyApp(const std::string& versionDir) {
    return versionIsInstalled(versionDir, RobloxApp::Player) ||
           versionIsInstalled(versionDir, RobloxApp::Studio);
}

std::string appSettingsXml() {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<Settings>\n"
           "\t<ContentFolder>content</ContentFolder>\n"
           "\t<BaseUrl>http://www.roblox.com</BaseUrl>\n"
           "</Settings>";
}

} // namespace tuxblox
