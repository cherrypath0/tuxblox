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
#include "cli.h"
#include "install_paths.h"
#include "console_ui.h"
#include "ui_cache.h"
#include "ui_extract.h"
#include "uninstall.h"
#include "version.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

namespace {
// Without --headless the interface is a window, and a window has no terminal to read failures from, so a missing graphical session is named up front.
bool hasGraphicalSession() {
    const char* display = getenv("DISPLAY");
    const char* wayland = getenv("WAYLAND_DISPLAY");
    if ((display && *display) || (wayland && *wayland)) {
        return true;
    }
    // A Wayland client falls back to this socket name when the variable is unset.
    const char* runtimeDir = getenv("XDG_RUNTIME_DIR");
    std::error_code ec;
    return runtimeDir && *runtimeDir && std::filesystem::exists(std::filesystem::path(runtimeDir) / "wayland-0", ec);
}

void reportError(const std::string& details) {
    fprintf(stderr, "Error: %s\n", details.c_str());
}

void printUninstallResult(bool ok, const char* failureText) {
    if (ok) {
        printf("TuxBlox has been completely removed from this system.\n");
    } else {
        fprintf(stderr, "Error: %s\n", failureText);
    }
}

// Runs the interface binary with `flag` and reports whether a window was shown. Waits for it, because this process has to outlive the window.
bool runInterfaceDialog(const std::string& uiBinaryPath, const char* flag) {
    const pid_t child = fork();
    if (child == 0) {
        execl(uiBinaryPath.c_str(), uiBinaryPath.c_str(), flag, static_cast<char*>(nullptr));
        _exit(127);
    }
    if (child < 0) {
        return false;
    }
    int status = 0;
    waitpid(child, &status, 0);
    return !(WIFEXITED(status) && WEXITSTATUS(status) == 127);
}

// Shows the finished-uninstall dialog, falling back to the terminal when there is no interface to show it with.
void showUninstallResult(bool ok, const char* failureText, const std::string& uiBinaryPath) {
    const char* flag = ok ? "--uninstall-result-ok" : "--uninstall-result-failed";
    if (uiBinaryPath.empty() || !runInterfaceDialog(uiBinaryPath, flag)) {
        printUninstallResult(ok, failureText);
    }
}
} // namespace

int main(int argc, char** argv) {
    using namespace tuxblox;

    const CliOptions options = parseArgs(argc, argv);
    if (!options.error.empty()) {
        fprintf(stderr, "%s\n\n%s", options.error.c_str(), usageText());
        return 2;
    }
    if (options.help) {
        printf("%s", usageText());
        return 0;
    }
    if (options.version) {
        printf("%s-%s\n", kTuxBloxVersion, kTuxBloxChannel);
        return 0;
    }

    if (geteuid() == 0) {
        if (!options.allowRoot) {
            fprintf(stderr, "TuxBlox: refusing to run as root. Re-run as a normal user, or pass "
                            "--allow-root if you have a specific reason.\n");
            return 1;
        }
        printf("WARNING: The current user is root, it is highly recommended to launch TuxBlox as "
               "a normal user unless there is a specific reason why\n");
        fflush(stdout);
    }

    if (!options.dir.empty()) {
        std::error_code ec;
        const std::filesystem::path path(options.dir);
        if (std::filesystem::exists(path, ec) && !std::filesystem::is_directory(path, ec)) {
            reportError("--dir names a file, not a folder: " + options.dir);
            return 2;
        }
        if (!std::filesystem::exists(path.parent_path(), ec)) {
            reportError("--dir's parent folder does not exist: " + options.dir);
            return 2;
        }
    }

    // --uninstall -- passed by the launcher's Settings tab. Never shows the
    // install UI, just does the removal and reports the result.
    if (options.uninstall) {
        // Unpacked before the removal, because the installer this reads the interface from lives inside the folder about to be deleted.
        std::string uiBinaryPath;
        if (!options.headless && hasGraphicalSession()) {
            const UiStackResult stack = ensureUiStack(selfExePath(), kTuxBloxVersion);
            if (stack.ok) {
                uiBinaryPath = stack.uiBinaryPath;
            }
        }

        const bool ok = performUninstall(options.dir.empty() ? installDir() : options.dir);
        const char* failureText =
            "Desktop shortcuts and URL handlers were removed, but the TuxBlox folder could "
            "not be fully deleted. You may need to remove it manually.";
        if (options.headless) {
            printUninstallResult(ok, failureText);
        } else {
            showUninstallResult(ok, failureText, uiBinaryPath);
        }

        // Last, and from this process rather than one living inside it: the interface was unpacked here, and the folder goes with the install.
        try {
            std::error_code cacheEc;
            std::filesystem::remove_all(uiCacheRoot(), cacheEc);
        } catch (const std::exception&) {
            // Nowhere to unpack to means nothing was ever unpacked, so there is nothing to remove.
        }
        return ok ? 0 : 1;
    }

    App app(options.channel, options.latest, options.dir);
    app.start();

    if (options.headless) {
        if (!runConsoleInstall(app)) {
            app.cancel(); // no-op if it already finished; unblocks a cancelled run
            return 1;
        }
    } else {
        if (!hasGraphicalSession()) {
            reportError("There is no graphical session to show the installer in. Run this installer from your desktop, or use --headless to install from the terminal.");
            app.cancel();
            return 1;
        }
        const UiStackResult stack = ensureUiStack(selfExePath(), kTuxBloxVersion);
        if (!stack.ok) {
            app.cancel();
            reportError(stack.errorMessage);
            if (!stack.errorDetail.empty()) {
                fprintf(stderr, "Details: %s\n", stack.errorDetail.c_str());
            }
            return 1;
        }
        // The interface runs the install itself, so this process's own pipeline stops here.
        app.cancel();
        std::vector<char*> args;
        args.push_back(const_cast<char*>(stack.uiBinaryPath.c_str()));
        for (int i = 1; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        args.push_back(nullptr);
        execv(stack.uiBinaryPath.c_str(), args.data());
        const int execError = errno;
        reportError("TuxBlox unpacked its interface but could not start it. This usually means the folder it was unpacked into does not allow programs to run. Try running this installer with --headless.");
        fprintf(stderr, "Details: %s: %s\n", stack.uiBinaryPath.c_str(), strerror(execError));
        return 1;
    }

    const std::string launcher = app.launcherPath();

    // --nolaunch -- the install is complete either way; the only difference
    // is that we stop here instead of handing off to the launcher.
    if (options.noLaunch) {
        // printf("Launcher not started (--nolaunch). Run it manually: %s\n", launcher.c_str());
        return 0;
    }

    // Carried across: a root install that did not pass this on would hand off to a launcher that refuses to start, right after saying the install worked.
    if (options.allowRoot) {
        execl(launcher.c_str(), launcher.c_str(), "--allow-root", (char*)nullptr);
    } else {
        execl(launcher.c_str(), launcher.c_str(), (char*)nullptr);
    }
    // Only reached if execl() failed -- the install itself already succeeded,
    // so say so rather than letting the window just vanish.
    reportError(
        "TuxBlox was installed successfully, but failed to launch " + launcher +
        ". You can try running it manually.");
    return 1;
}
