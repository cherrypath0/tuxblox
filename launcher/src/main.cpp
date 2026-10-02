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

#include "app.h"
#include "adw_env.h"
#include "ui_adw/launcher_window.h"
#include "ui_adw/message_box.h"
#include "install_paths.h"
#include "root_guard.h"
#include "headless_launch.h"
#include "watch_launch.h"
#include "prefix_file_bridge.h"
#include "copyright_file.h"
#include "license_file.h"
#include "desktop_integration.h"
#include "single_instance.h"
#include "app_scope.h"
#include "system_requirements.h"
#include "version.h"
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace {

constexpr const char* kErrorTitle = "TuxBlox has encountered an error and has to quit!";

bool startsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

} // namespace

int main(int argc, char** argv) {
    using namespace tuxblox;

    // Before the scope is joined and before any argument is read: a root run
    // builds a virtual drive the real user cannot write to afterwards.
    const bool allowRootRequested = takeAllowRootFlag(argc, argv);
    if (!rootRunAllowed(allowRootRequested, geteuid())) {
        return 1;
    }
    setAllowRoot(allowRootRequested);

    // Before any mode branches below: every one of them either becomes the
    // long-lived process the desktop will display (GUI, --watch-launch) or
    // exec()s into Proton while staying in this cgroup (the headless
    // quick-launch paths), and children inherit whatever scope we join here.
    // Doing it once, first, covers all of them. See app_scope.h for why the
    // launch method otherwise decides whether TuxBlox is identifiable at all.
    ensureAppScope();

    // Answered before anything else, and before the GUI: this is the same
    // question the launcher asks every other TuxBlox binary to decide
    // whether an install is of a piece, so it has to be cheap and it has
    // to be the build's own answer. Ahead of the requirements check below for
    // the same reason -- an install must still be able to say what it is.
    if (argc > 1 && std::string(argv[1]) == "--version") {
        printf("%s\n", kTuxBloxBuildId);
        return 0;
    }

    // Every route to Roblox passes through here -- the window, a roblox: link, a desktop shortcut, a
    // place file -- so one check covers all of them. It refuses only what it can prove: see
    // system_requirements.h for why being unable to read a version never stops a launch.
    {
        const std::vector<UnmetRequirement> unmet = unmetRequirements(collectSystemFacts());
        if (!unmet.empty()) {
            showErrorMessageBox("TuxBlox is not supported on your system", unsupportedSystemMessage(unmet));
            return 1;
        }
    }

    if (argc > 1) {
        std::string arg1 = argv[1];

        LaunchTarget target = LaunchTarget::Player;
        std::string uri;
        bool headless = false;

        if (startsWith(arg1, "roblox-player:") || startsWith(arg1, "roblox:")) {
            headless = true; target = LaunchTarget::Player; uri = arg1;
        } else if (startsWith(arg1, "roblox-studio-auth:") || startsWith(arg1, "roblox-studio:")) {
            headless = true; target = LaunchTarget::Studio; uri = arg1;
        } else if (arg1 == "--launch-player") {
            headless = true; target = LaunchTarget::Player;
        } else if (arg1 == "--launch-studio") {
            headless = true; target = LaunchTarget::Studio;
        }

        if (headless) {
            std::string dir;
            try {
                dir = installDir();
            } catch (const std::exception& e) {
                fprintf(stderr, "TuxBlox: %s\n", e.what());
                return 1;
            }
            return runHeadlessQuickLaunch(dir, target, uri);
        }

        // Spawned by App::requestLaunch() as a fully detached process -- see
        // its own comment for why the GUI hands off to this instead of
        // staying open (backgrounded or otherwise). Unlike the headless
        // quick-launch paths above (which execv()-replace themselves into
        // Proton and report nothing further), this one waits for the
        // process and shows a crash popup if warranted.
        if (arg1 == "--watch-launch" && argc > 2) {
            std::string arg2 = argv[2];
            LaunchTarget watchTarget;
            if (arg2 == "player") watchTarget = LaunchTarget::Player;
            else if (arg2 == "studio") watchTarget = LaunchTarget::Studio;
            else { fprintf(stderr, "TuxBlox: --watch-launch needs 'player' or 'studio'\n"); return 1; }

            std::string dir;
            try {
                dir = installDir();
            } catch (const std::exception& e) {
                fprintf(stderr, "TuxBlox: %s\n", e.what());
                return 1;
            }
            return runWatchAndLaunch(dir, watchTarget, "", kTuxBloxVersion);
        }

        // Launched from an exported Wine shortcut (see wine_shortcut_export.h).
        // The recorded path only picks the target -- the actual exe is
        // re-resolved through resolveActiveVersionExePath()/
        // resolveOrBootstrapExePath(), so a shortcut written against an old
        // version-<hash> keeps working after Roblox updates.
        if (arg1 == "--run-exe" && argc > 2) {
            const std::string exeArg = argv[2];
            const size_t slash = exeArg.find_last_of("\\/");
            std::string leaf = slash == std::string::npos ? exeArg : exeArg.substr(slash + 1);
            for (char& c : leaf) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

            LaunchTarget runTarget;
            if (leaf == "robloxstudiobeta.exe") runTarget = LaunchTarget::Studio;
            else if (leaf == "robloxplayerbeta.exe") runTarget = LaunchTarget::Player;
            else {
                fprintf(stderr, "TuxBlox: --run-exe only accepts RobloxStudioBeta.exe "
                                "or RobloxPlayerBeta.exe, got '%s'\n", leaf.c_str());
                return 1;
            }

            std::string dir;
            try {
                dir = installDir();
            } catch (const std::exception& e) {
                fprintf(stderr, "TuxBlox: %s\n", e.what());
                return 1;
            }
            return runWatchAndLaunch(dir, runTarget, "", kTuxBloxVersion);
        }

        // Launched by the desktop's MIME handler for .rbxl/.rbxlx -- item 18.
        // The file lives outside the prefix, which has no Z: drive (plan.txt
        // item 20), so it has to be bridged in before Studio can open it.
        if (arg1 == "--open-file" && argc > 2) {
            std::string dir;
            try {
                dir = installDir();
            } catch (const std::exception& e) {
                fprintf(stderr, "TuxBlox: %s\n", e.what());
                return 1;
            }

            const std::string winPath = bridgeHostPathIntoPrefix(dir, argv[2]);
            if (winPath.empty()) {
                showErrorMessageBox("TuxBlox Error",
                                    std::string("Could not open this file in Roblox Studio:\n") +
                                        argv[2]);
                return 1;
            }
            return runWatchAndLaunch(dir, LaunchTarget::Studio, winPath, kTuxBloxVersion);
        }
    }

    // GUI mode.
    std::string dir;
    try {
        dir = installDir();
    } catch (const std::exception& e) {
        fprintf(stderr, "%s\nDetails: %s\n", kErrorTitle, e.what());
        return 1;
    }

    // Best-effort: the installer normally creates this directory first, but
    // the launcher must be self-sufficient (e.g. a manually built/copied
    // binary run before any installer has). Downstream best-effort steps
    // (writeCopyrightFile, ensureLicenseFile, ensureDesktopIntegration)
    // already tolerate a still-missing directory, so a failure here is not
    // fatal.
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }

    // Only the GUI itself is single-instance: the quick-launch and --watch-launch paths above have already returned.
    if (!acquireSingleInstanceLock(dir)) {
        showErrorMessageBox("TuxBlox Error", "An instance of the TuxBlox Launcher is already running!");
        return 1;
    }

    std::string exePath = selfExePath();
    if (exePath.empty()) {
        // selfExePath() is readlink("/proc/self/exe") -- on Linux this cannot
        // fail for the calling process as long as /proc is mounted, so this
        // branch is unreachable in practice. This literal is NOT guaranteed
        // to match the installer's actual layout: since the launcher became
        // a bundled archive artifact, an installed launcher's real path is
        // dir + "/launcher/TuxBloxLauncher", not dir + "/TuxBloxLauncher"
        // directly (see installer/src/installer_steps.cpp's kLauncherExeName
        // resolution). Left as a last-resort guess rather than fixed to
        // match, since the only consumer (App::launcherExePath_, used solely
        // by the --watch-launch self-re-exec) would fail loudly at execl on
        // a wrong path either way, not silently misbehave.
        exePath = dir + "/TuxBloxLauncher";
    }

    writeCopyrightFile(dir);
    ensureLicenseFile(dir);

    // App's constructor is cheap (just a JSON load + a filesystem::exists
    // check) -- constructing it before the window is shown doesn't
    // reintroduce the "window appears immediately" concern Finding 5 (2026-
    // 07-28 final review) was about. That concern is specifically about
    // ensureDesktopIntegration()'s up-to-~12s xdg-mime/update-desktop-
    // database calls, which stays after window.show() below, same as before.
    App app(dir, kTuxBloxBuildId, exePath);

    // With Auto-Update on the answer decides whether a window is wanted at all, so it is worth waiting
    // for one, and an update about to install never flashes the Home screen up first. With it off the
    // update is only being offered, so the check belongs behind the window, which starts it the moment
    // it opens -- starting it here as well would mean a run with no display to open had to wait for a
    // network request it was never going to use.
    if (app.snapshot().settings.autoUpdate) {
        app.startUpdateCheck();
        app.waitForUpdateCheck(std::chrono::seconds(5));
    }

    int windowStatus = 0;
    if (!app.needsInstallerHandoff()) {
        // Before the window exists: some compositors (observed on KDE Plasma/KWin) match a new window to its .desktop entry once, at creation, and never retry
        writeDesktopEntries(exePath);

        // Kept for the life of the window, since GTK can re-read fontconfig while it runs; programs started from the window are given the environment without it
        useBundledEnvironment(interfaceStackRoot());
        windowStatus = runLauncherWindow(app, exePath, dir);
        restoreBundledEnvironment();
    }
    if (windowStatus != 0 && !app.needsInstallerHandoff() && !app.needsUninstallHandoff()) return windowStatus;

    if (app.needsUninstallHandoff()) {
        // Same handoff shape as an update, but with --uninstall instead of
        // --channel: the installer removes the install folder and this
        // launcher's desktop/URL-handler registrations, then shows its own
        // confirmation popup. This process never returns on success.
        std::string installerPath = app.installerHandoffPath();
        // Naming the folder explicitly, so the installer cannot act on a
        // different install from the one that asked for it.
        std::string dir = installDir();
        char* installerArgv[] = {
            const_cast<char*>(installerPath.c_str()),
            const_cast<char*>("--uninstall"),
            const_cast<char*>("--dir"),
            const_cast<char*>(dir.c_str()),
            allowRoot() ? const_cast<char*>("--allow-root") : nullptr,
            nullptr
        };
        execv(installerPath.c_str(), installerArgv);
        // Only reached if execv() itself failed.
        fprintf(stderr, "TuxBlox: uninstaller ready but failed to launch it at %s\n", installerPath.c_str());
        return 1;
    }

    if (app.needsInstallerHandoff()) {
        // The installer, run against this already-existing install
        // directory, auto-detects upgrade mode: it replaces only
        // proton/ and the Launcher/Installer binaries (never wiping
        // runtime/ or anything else), showing its own "Upgrading
        // Proton"/"Upgrading TuxBlox" progress, then re-execs the launcher
        // when done -- this process never returns on success.
        std::string installerPath = app.installerHandoffPath();
        // Carries the user's selected update channel across the handoff --
        // without this, the installer would fall back to its own default
        // channel and silently switch the user off whatever they picked in
        // Settings.
        std::string channel = app.snapshot().settings.channel;
        std::string dir = installDir();
        char* installerArgv[] = {
            const_cast<char*>(installerPath.c_str()),
            const_cast<char*>("--channel"),
            const_cast<char*>(channel.c_str()),
            const_cast<char*>("--dir"),
            const_cast<char*>(dir.c_str()),
            allowRoot() ? const_cast<char*>("--allow-root") : nullptr,
            nullptr
        };
        execv(installerPath.c_str(), installerArgv);
        // Only reached if execv() itself failed -- the installer binary is
        // in place on disk either way, so the next manual launch (or a
        // user manually running TuxBloxInstaller) still picks up the update.
        fprintf(stderr, "TuxBlox: update ready but failed to launch the updater at %s\n", installerPath.c_str());
        return 1;
    }

    return 0;
}
