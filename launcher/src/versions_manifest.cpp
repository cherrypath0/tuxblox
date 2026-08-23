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

// Iterates the prefix's Versions/ directory. Returns false only if the
// directory exists but couldn't be read -- callers must not treat that as
// "nothing installed". A missing directory is a successful empty scan
// (the prefix simply has no Roblox in it yet).
bool scanPrefixVersionsInto(const std::string& installDir, LaunchTarget target,
                            std::vector<std::string>& out) {
    const std::string versionsDir = prefixVersionsDir(installDir);
    std::error_code ec;
    if (!fs::exists(versionsDir, ec) || ec) return true;

    for (const auto& entry : fs::directory_iterator(versionsDir, ec)) {
        if (ec) return false;
        std::error_code entryEc;
        if (!entry.is_directory(entryEc) || entryEc) continue;
        // Player and Studio versions live in the SAME shared Versions/
        // directory, so a directory only belongs to `target` if it actually
        // contains that target's own exe.
        if (!fs::exists(entry.path() / targetExeName(target), entryEc) || entryEc) continue;
        out.push_back(entry.path().filename().string());
    }
    return !ec;
}

// Most recently modified of `hashes`, used to pick a pin when the manifest
// has none (or names a version that's gone). Falls back to the first entry
// if the timestamps can't be read.
std::string newestVersion(const std::string& installDir, const std::vector<std::string>& hashes) {
    std::string best;
    fs::file_time_type bestTime{};
    for (const auto& hash : hashes) {
        std::error_code ec;
        auto t = fs::last_write_time(fs::path(prefixVersionsDir(installDir)) / hash, ec);
        if (ec) continue;
        if (best.empty() || t > bestTime) {
            best = hash;
            bestTime = t;
        }
    }
    return best.empty() ? hashes.front() : best;
}

void reconcileTargetWithPrefix(const std::string& installDir, VersionsManifest& manifest,
                               LaunchTarget target) {
    std::vector<std::string> hashes;
    if (!scanPrefixVersionsInto(installDir, target, hashes)) return; // unreadable -- keep what we had

    AppVersions& av = appVersionsFor(manifest, target);
    auto onDisk = [&](const std::string& hash) {
        return std::find(hashes.begin(), hashes.end(), hash) != hashes.end();
    };

    // Drop what's no longer on disk (versions deleted outside the launcher,
    // or a wiped prefix), keeping the surviving entries' channel/installedAt.
    av.installed.erase(std::remove_if(av.installed.begin(), av.installed.end(),
                                       [&](const InstalledVersion& v) { return !onDisk(v.hash); }),
                       av.installed.end());

    // Add what the manifest never knew about -- a deleted versions.json, or
    // a version the official installer bootstrapped behind our back.
    for (const auto& hash : hashes) {
        bool known = std::any_of(av.installed.begin(), av.installed.end(),
                                  [&](const InstalledVersion& v) { return v.hash == hash; });
        if (!known) av.installed.push_back({hash, "live", ""}); // installedAt unknown -- installer-driven
    }

    if (!av.activeHash.empty() && !onDisk(av.activeHash)) av.activeHash.clear();
    if (av.activeHash.empty() && !hashes.empty()) av.activeHash = newestVersion(installDir, hashes);
    if (!hashes.empty()) av.bootstrapped = true;
}

} // namespace

std::string prefixVersionsDir(const std::string& installDir) {
    // NOTE: hardcodes "users/user/..." matching this codebase's current
    // convention (see roblox_log_capture.cpp) -- if the separate
    // Wine-per-user-paths plan lands, this needs the resolved username.
    return installDir + "/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
}

std::vector<std::string> scanPrefixVersions(const std::string& installDir, LaunchTarget target) {
    std::vector<std::string> hashes;
    scanPrefixVersionsInto(installDir, target, hashes);
    return hashes;
}

void reconcileWithPrefix(const std::string& installDir, VersionsManifest& manifest) {
    reconcileTargetWithPrefix(installDir, manifest, LaunchTarget::Player);
    reconcileTargetWithPrefix(installDir, manifest, LaunchTarget::Studio);
}

VersionsManifest loadInstalledVersions(const std::string& installDir) {
    VersionsManifest m = loadVersionsManifest(installDir);
    reconcileWithPrefix(installDir, m);
    return m;
}

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
    VersionsManifest manifest = loadVersionsManifest(installDir);
    const AppVersions before = appVersionsFor(manifest, target);
    reconcileWithPrefix(installDir, manifest);
    const AppVersions& after = appVersionsFor(manifest, target);
    if (after.installed.empty()) return; // installer run produced nothing -- don't mark bootstrapped

    // Only write when the reconcile actually changed this target, so a
    // detached watcher process doesn't rewrite the file on every launch.
    if (before.activeHash == after.activeHash && before.bootstrapped == after.bootstrapped &&
        before.installed.size() == after.installed.size()) {
        bool same = true;
        for (size_t i = 0; i < after.installed.size(); ++i) {
            if (before.installed[i].hash != after.installed[i].hash) { same = false; break; }
        }
        if (same) return;
    }
    saveVersionsManifest(installDir, manifest);
}

} // namespace tuxblox
