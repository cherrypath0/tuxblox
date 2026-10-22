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

#include "roblox_autoupdate.h"
#include "prefix_user.h"

#include "desktop_notify.h"
#include "versions_manifest.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

namespace {

// Roblox runs from inside its own version directory, so removing one still in use would pull the
// exe out from under a window the user has open -- a second Studio, most often.
bool versionInUse(const std::string& versionDir) {
    std::error_code error;
    const std::string prefix = versionDir + "/";
    for (const auto& entry : std::filesystem::directory_iterator("/proc", error)) {
        const std::string pid = entry.path().filename().string();
        if (pid.empty() || !std::isdigit(static_cast<unsigned char>(pid[0]))) continue;
        const std::string exe = std::filesystem::read_symlink(entry.path() / "exe", error).string();
        if (error) { error.clear(); continue; }
        if (exe.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

} // namespace

std::optional<std::string> ownExePath() {
    char buf[4096];
    const ssize_t length = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (length <= 0) return std::nullopt;
    buf[length] = '\0';
    return std::string(buf);
}

std::string robloxVersionsDir(const std::string& installDir) {
    return prefixUserDir(installDir) + "/AppData/Local/Roblox/Versions";
}

bool shouldRunUpdateCheck(const Settings& settings) {
    return settings.autoUpdateRoblox;
}

bool canRunUpdateCheck(const Settings& settings, const std::optional<std::string>& exe) {
    return shouldRunUpdateCheck(settings) && exe.has_value();
}

std::vector<std::string> updateEnvironment(const std::string& installDir, LaunchTarget target) {
    return {
        std::string("TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP=") +
            (target == LaunchTarget::Player ? "player" : "studio"),
        "TUXBLOX_BOOTSTRAPPER_INSTALL_DIR=" + robloxVersionsDir(installDir),
    };
}

void runRobloxUpdateCheck(const std::string& installDir, LaunchTarget target,
                           const Settings& settings) {
    const std::optional<std::string> exe = ownExePath();
    if (!canRunUpdateCheck(settings, exe)) {
        // Only worth a word when the setting asked for a check that cannot be run at all.
        if (shouldRunUpdateCheck(settings)) {
            fprintf(stderr, "TuxBlox: could not find this launcher's own path, skipping the "
                             "Roblox update check\n");
        }
        return;
    }

    // Taken before the update so the version it installs can be told apart from the ones already there.
    const std::vector<std::string> versionsBefore = scanPrefixVersions(installDir, target);

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "TuxBlox: could not start the update check, launching anyway\n");
        return;
    }
    if (pid == 0) {
        for (const auto& pair : updateEnvironment(installDir, target)) {
            const auto split = pair.find('=');
            setenv(pair.substr(0, split).c_str(), pair.substr(split + 1).c_str(), 1);
        }
        // No --allow-root: main() hands these flags over above the root guard, so the child never asks.
        const char* argv[] = {exe->c_str(), "--update-roblox", nullptr};
        execv(exe->c_str(), const_cast<char* const*>(argv));
        _exit(127); // only reached if execv failed
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "TuxBlox: the Roblox update check did not finish, launching anyway\n");
        // The check runs without a window of its own, so there is nothing on
        // screen for this to have appeared on. It says the update was skipped
        // rather than that Roblox failed to start: the launch goes ahead on
        // the version already installed either way.
        showDesktopNotification(
            "Failed to Update Roblox",
            "Something went wrong while attempting to update Roblox, so the update "
            "was skipped. Please check your internet connection and try again.");
        return;
    }

    activateNewlyUpdatedVersion(installDir, target, versionsBefore);

    // Whatever the update installed is the one to keep; empty means it installed nothing.
    const std::string installed =
        newestAddedVersion(installDir, versionsBefore, scanPrefixVersions(installDir, target));
    removeSupersededVersions(installDir, target, installed);
}

void activateNewlyUpdatedVersion(const std::string& installDir, LaunchTarget target,
                                  const std::vector<std::string>& versionsBefore) {
    const std::string installed =
        newestAddedVersion(installDir, versionsBefore, scanPrefixVersions(installDir, target));
    // Nothing new means the update had nothing to do, and whichever version is active stays active.
    if (installed.empty()) return;

    // Re-read rather than reuse anything held earlier: the update has just changed the prefix from
    // another process, and the launcher may be running and holding its own copy.
    VersionsManifest manifest = loadInstalledVersions(installDir);
    AppVersions& versions = appVersionsFor(manifest, target);
    if (versions.activeHash == installed) return;

    versions.activeHash = installed;
    saveVersionsManifest(installDir, manifest);
}

void removeSupersededVersions(const std::string& installDir, LaunchTarget target,
                               const std::string& keepHash) {
    // Nothing was installed, so nothing here is superseded and the user's own choice stands.
    if (keepHash.empty()) return;

    const std::string versionsDir = robloxVersionsDir(installDir);
    bool removedAny = false;

    for (const std::string& hash : scanPrefixVersions(installDir, target)) {
        if (hash == keepHash) continue;

        const std::string dir = versionsDir + "/" + hash;
        if (versionInUse(dir)) continue;

        std::error_code error;
        std::filesystem::remove_all(dir, error);
        if (error) continue;
        // The download is extracted in place and its archive is left behind, so it goes too.
        std::filesystem::remove(dir + ".zip", error);
        removedAny = true;
    }

    // Re-reading reconciles versions.json against the prefix, which is what drops the entries whose
    // directories have just gone and re-pins the active one if it was among them.
    if (removedAny) saveVersionsManifest(installDir, loadInstalledVersions(installDir));
}

} // namespace tuxblox
