#include "app.h"
#include "ui.h"
#include "install_paths.h"
#include "headless_launch.h"
#include "copyright_file.h"
#include "desktop_integration.h"
#include "version.h"
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>

namespace {

constexpr const char* kErrorTitle = "TuxBlox has encountered an error and has to quit!";

// Resolves the path of the currently-running binary via /proc/self/exe --
// this is what a self-update rename()s over and what execv() re-launches,
// so it must be the binary's real on-disk path, not argv[0] (which can be
// a relative path, "TuxBloxLauncher", or missing entirely depending on how
// the .desktop Exec= line or a shell invoked it).
std::string selfExePath() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf);
}

bool startsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

} // namespace

int main(int argc, char** argv) {
    using namespace tuxblox;

    if (argc > 1) {
        std::string arg1 = argv[1];
        LaunchTarget target = LaunchTarget::Player;
        std::string uri;
        bool headless = false;

        if (startsWith(arg1, "roblox-player:")) {
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
    // (writeCopyrightFile, ensureDesktopIntegration) already tolerate a
    // still-missing directory, so a failure here is not fatal.
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }

    std::string exePath = selfExePath();
    if (exePath.empty()) {
        exePath = dir + "/TuxBloxLauncher"; // fallback -- matches the installer's canonical install path
    }

    writeCopyrightFile(dir);

    // Ui::init() runs before anything that can block, so the window appears
    // immediately -- see Finding 5, 2026-07-28 final review. In particular,
    // ensureDesktopIntegration() below is bounded but can take up to ~12s
    // (xdg-mime/update-desktop-database calls), and App's constructor no
    // longer calls it internally for exactly this reason.
    Ui ui;
    if (!ui.init()) {
        fprintf(stderr, "%s\nDetails: Failed to initialize launcher UI (SDL2/OpenGL)\n", kErrorTitle);
        return 1;
    }

    ensureDesktopIntegration(dir, exePath);

    App app(dir, kTuxBloxVersion);
    app.startUpdateCheck();

    bool running = true;
    while (running && !app.needsInstallerHandoff()) {
        running = ui.renderFrame(app);
    }

    ui.shutdown();

    if (app.needsInstallerHandoff()) {
        // The installer, run against this already-existing install
        // directory, auto-detects upgrade mode: it replaces only
        // ProtonBuild/ and the Launcher/Installer binaries (never wiping
        // runtime/ or anything else), showing its own "Upgrading
        // Proton"/"Upgrading TuxBlox" progress, then re-execs the launcher
        // when done -- this process never returns on success.
        std::string installerPath = app.installerHandoffPath();
        char* installerArgv[] = {const_cast<char*>(installerPath.c_str()), nullptr};
        execv(installerPath.c_str(), installerArgv);
        // Only reached if execv() itself failed -- the installer binary is
        // in place on disk either way, so the next manual launch (or a
        // user manually running TuxBloxInstaller) still picks up the update.
        fprintf(stderr, "TuxBlox: update ready but failed to launch the updater at %s\n", installerPath.c_str());
        return 1;
    }

    return 0;
}
