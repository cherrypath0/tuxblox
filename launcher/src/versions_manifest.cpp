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

#include "versions_manifest.h"
#include "json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string manifestFilePath(const std::string& installDir) {
    return installDir + "/versions.json";
}

nlohmann::json toJson(const AppVersions& v) {
    nlohmann::json j;
    j["active"] = v.activeHash;
    j["bootstrapped"] = v.bootstrapped;
    j["installed"] = nlohmann::json::array();
    for (const auto& iv : v.installed) {
        j["installed"].push_back({
            {"hash", iv.hash},
            {"channel", iv.channel},
            {"installed_at", iv.installedAt},
        });
    }
    return j;
}

AppVersions fromJson(const nlohmann::json& j) {
    AppVersions v;
    v.activeHash = j.value("active", std::string());
    v.bootstrapped = j.value("bootstrapped", false);
    for (const auto& e : j.value("installed", nlohmann::json::array())) {
        v.installed.push_back({
            e.at("hash").get<std::string>(),
            e.value("channel", std::string()),
            e.value("installed_at", std::string()),
        });
    }
    return v;
}

} // namespace

VersionsManifest loadVersionsManifest(const std::string& installDir) {
    VersionsManifest m;
    try {
        std::ifstream file(manifestFilePath(installDir));
        if (!file) return m;

        nlohmann::json j;
        file >> j;
        m.player = fromJson(j.at("player"));
        m.studio = fromJson(j.at("studio"));
        return m;
    } catch (...) {
        // Missing file, unreadable file, parse error, or a malformed field --
        // fall back to defaults wholesale, same contract as settings.cpp's
        // loadSettings. A corrupt versions.json must never crash the launcher.
        return VersionsManifest{};
    }
}

void saveVersionsManifest(const std::string& installDir, const VersionsManifest& manifest) {
    try {
        std::error_code ec;
        fs::create_directories(installDir, ec);

        nlohmann::json j;
        j["player"] = toJson(manifest.player);
        j["studio"] = toJson(manifest.studio);

        std::ofstream file(manifestFilePath(installDir), std::ios::binary);
        if (!file) return;
        file << j.dump(2);
    } catch (...) {
        // Best-effort -- a failed save must not crash the launcher.
    }
}

AppVersions& appVersionsFor(VersionsManifest& manifest, LaunchTarget target) {
    return target == LaunchTarget::Player ? manifest.player : manifest.studio;
}

const AppVersions& appVersionsFor(const VersionsManifest& manifest, LaunchTarget target) {
    return target == LaunchTarget::Player ? manifest.player : manifest.studio;
}

void registerBootstrappedVersion(const std::string& installDir, LaunchTarget target) {
    const std::string versionsDir =
        installDir + "/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
    std::error_code ec;
    if (!fs::exists(versionsDir, ec) || ec) return;

    VersionsManifest manifest = loadVersionsManifest(installDir);
    AppVersions& av = appVersionsFor(manifest, target);

    std::vector<std::string> newHashes;
    for (const auto& entry : fs::directory_iterator(versionsDir, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        bool alreadyKnown = std::any_of(av.installed.begin(), av.installed.end(),
                                         [&](const InstalledVersion& v) { return v.hash == name; });
        if (alreadyKnown) continue;
        // Player and Studio versions live in the SAME shared Versions/
        // directory, so a directory not yet known to THIS target's
        // installed list isn't necessarily new -- it might just belong to
        // the other target (e.g. Player already bootstrapped and Studio is
        // bootstrapping now). Only count it as newly discovered for
        // `target` if it actually contains that target's own exe.
        std::error_code existsEc;
        if (!fs::exists(entry.path() / targetExeName(target), existsEc) || existsEc) continue;
        newHashes.push_back(name);
    }
    if (newHashes.empty()) return;

    for (const auto& hash : newHashes) {
        av.installed.push_back({hash, "live", ""}); // installedAt left empty -- exact time unknown, installer-driven
    }
    av.bootstrapped = true;
    if (av.activeHash.empty() && newHashes.size() == 1) {
        av.activeHash = newHashes[0];
    }
    saveVersionsManifest(installDir, manifest);
}

} // namespace tuxblox
