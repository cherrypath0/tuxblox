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
#include "ui_qt/icon_utils.h"
#include "ui_qt/main_window.h"
#include "ui_qt/message_box.h"
#include "ui_qt/theme.h"
#include "install_paths.h"
#include "headless_launch.h"
#include "watch_launch.h"
#include "prefix_file_bridge.h"
#include "wine_shortcut_export.h"
#include "copyright_file.h"
#include "license_file.h"
#include "desktop_integration.h"
#include "single_instance.h"
#include "app_scope.h"
#include "version.h"
#include "inter_regular_ttf.h"  // generated at build time: kInterRegularTtf[]/Len
#include "inter_semibold_ttf.h" // generated at build time: kInterSemiBoldTtf[]/Len
#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>

namespace {

constexpr const char* kErrorTitle = "TuxBlox has encountered an error and has to quit!";

bool startsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

// Registers the embedded Inter TTFs with Qt's font database and returns the
// family name Qt assigned them (fontsource's internal name metadata, not
// necessarily the literal string "Inter") -- must be called after a
// QApplication/QGuiApplication instance exists, since the font database
// isn't available before then. Falls back to "Inter" (a reasonable guess,
// and harmless even if wrong -- Qt just falls back to its default font) if
// registration fails, e.g. corrupt/truncated embedded bytes.
QString registerInterFont() {
    QFontDatabase::addApplicationFontFromData(QByteArray(reinterpret_cast<const char*>(kInterSemiBoldTtf),
                                                           static_cast<int>(kInterSemiBoldTtfLen)));
    int id = QFontDatabase::addApplicationFontFromData(
        QByteArray(reinterpret_cast<const char*>(kInterRegularTtf), static_cast<int>(kInterRegularTtfLen)));
    if (id < 0) return "Inter";
    QStringList families = QFontDatabase::applicationFontFamilies(id);
    return families.isEmpty() ? "Inter" : families.first();
}

} // namespace

int main(int argc, char** argv) {
    // Required for a Qt resource file (launcher/resources/launcher.qrc)
    // compiled into a STATIC library (launcher_ui_qt) rather than directly
    // into this executable: the linker only pulls an archive member into
    // the final binary if something references a symbol from it, and
    // nothing calls a function from the generated qrc_launcher.cpp
    // translation unit directly -- only its static initializer would run,
    // and only if that .o actually got linked in. Without this call, every
    // QIcon(":/...")/QPixmap(":/...") construction silently returns a
    // null/empty image (no console warning from QIcon itself, unlike
    // QPixmap::scaled()'s explicit "null pixmap" warning) instead of
    // failing loudly -- observed for real: every sidebar/about-tab icon and
    // the window icon rendered as blank. Q_INIT_RESOURCE(launcher) forces
    // the resource registration to run regardless of what else got linked;
    // "launcher" matches launcher.qrc's own basename, which is what
    // CMAKE_AUTORCC names the generated qInitResources_launcher() function
    // after. Must be called from global scope (not inside a namespace),
    // hence its placement here rather than deeper in App/MainWindow.
    Q_INIT_RESOURCE(launcher);

    using namespace tuxblox;

    // Before any mode branches below: every one of them either becomes the
    // long-lived process the desktop will display (GUI, --watch-launch) or
    // exec()s into Proton while staying in this cgroup (the headless
    // quick-launch paths), and children inherit whatever scope we join here.
    // Doing it once, first, covers all of them. See app_scope.h for why the
    // launch method otherwise decides whether TuxBlox is identifiable at all.
    ensureAppScope();

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

    // Only the GUI itself is single-instance -- the headless quick-launch
    // and --watch-launch paths above already returned before reaching here,
    // so a launch-and-watch helper running in the background never trips
    // this. Same SDL_Init(SDL_INIT_VIDEO)/SDL_ShowSimpleMessageBox/SDL_Quit
    // shape as the crash popups in watch_launch.cpp -- SDL_ShowSimpleMessageBox
    // needs a video subsystem already up to pick a real backend, or it just
    // hangs with no dialog ever appearing instead of failing loudly.
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
    App app(dir, kTuxBloxVersion, exePath);

    // QGuiApplication::setDesktopFileName() is Qt's native equivalent of
    // the old SDL_VIDEO_X11_WMCLASS env var trick (ui.cpp's Ui::init(),
    // now removed) -- both exist so a pinned taskbar icon's running window
    // matches back to the .desktop entry's StartupWMClass
    // (ensureDesktopIntegration() below writes "tuxblox-launcher" there).
    QApplication qapp(argc, argv);
    qapp.setDesktopFileName("tuxblox-launcher");
    // Application-wide default so every top-level window (not just
    // MainWindow, which also sets its own below) gets the TuxBlox icon
    // instead of Qt's generic fallback -- this is also what X11/xcb
    // publishes as _NET_WM_ICON for the taskbar/dock entry. Uses
    // multiSizeWindowIcon(), not a plain QIcon(path) -- see its own doc
    // comment: a single-size icon was observed to make Qt's xcb backend
    // publish an empty (zero-data) _NET_WM_ICON property instead of the
    // actual image.
    qapp.setWindowIcon(tuxblox::multiSizeWindowIcon(":/branding/tuxblox_window_icon.png"));

    // Must happen before setStyleSheet() below: QSS "font-weight: 600"
    // rules only resolve to the embedded SemiBold weight if that weight is
    // already registered by the time the stylesheet is applied.
    QString interFamily = registerInterFont();
    QFont appFont(interFamily);
    qapp.setFont(appFont);

    qapp.setStyleSheet(tuxblox::theme::stylesheet());

    // Before the window is shown, not after: some window managers/
    // compositors (observed on KDE Plasma/KWin) resolve a new window's
    // taskbar/titlebar icon by matching its app_id/WM_CLASS against an
    // installed .desktop file exactly once, at window-creation time, and
    // never retry -- if that file doesn't exist yet, the icon stays blank
    // for the window's whole lifetime even once ensureDesktopIntegration()
    // (below) writes it moments later. writeDesktopEntries() is the fast,
    // synchronous subset of that work (just the icon PNG + .desktop file
    // writes); the slower xdg-mime/database-refresh calls stay in
    // ensureDesktopIntegration() below, after show(), exactly as before.
    writeDesktopEntries(exePath);

    MainWindow window(app);
    window.show();

    ensureDesktopIntegration(exePath);

    app.startUpdateCheck();

    qapp.exec();

    if (app.needsUninstallHandoff()) {
        // Same handoff shape as an update, but with --uninstall instead of
        // --channel: the installer removes ~/.tuxblox and this launcher's
        // desktop/URL-handler registrations, then shows its own
        // confirmation popup. This process never returns on success.
        std::string installerPath = app.installerHandoffPath();
        char* installerArgv[] = {
            const_cast<char*>(installerPath.c_str()),
            const_cast<char*>("--uninstall"),
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
        char* installerArgv[] = {
            const_cast<char*>(installerPath.c_str()),
            const_cast<char*>("--channel"),
            const_cast<char*>(channel.c_str()),
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
