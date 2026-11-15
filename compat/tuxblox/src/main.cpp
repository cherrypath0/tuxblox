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

// Portions derived from Valve's Proton:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <signal.h>
#include <unistd.h>

#include "release_stamp.h"
#include "support/flag_startup.h"
#include "prefix/prefix.h"
#include "support/account_name.h"
#include "launch/proton.h"
#include "launch/session.h"
#include "support/log_limit.h"
#include "support/util.h"

#include "embedded_data.h"

#ifndef TUXBLOX_VERSION
#define TUXBLOX_VERSION "unknown"
#endif

namespace {

// Version variables
const std::string TuxBloxVersion = TUXBLOX_VERSION;

const std::array<std::string, 6> RobloxProcesses = {
    // Clients
    "RobloxPlayerBeta.exe",
    "RobloxStudioBeta.exe",

    // Installers
    // The installers are no longer used in favor of the new custom bootstrapper, still kept here just in case
    "RobloxPlayerInstaller.exe",
    "RobloxStudioInstaller.exe",

    // Roblox Crash handler and the Studio MCP Server
    "RobloxCrashHandler.exe",
    "StudioMCP.exe",
};

const std::array<std::string, 9> HelpOutput = {
    "Options:",
    "--help                  Show this help message",
    "--version               Show the TuxBlox release this was published in",
    "--component-version     Show the layer's own version",
    "--immediate             Run TuxBlox without draining the prefix, must be used with the \"run\" argument",
    "--destroy               Destroys the prefix",
    "--allow-root            Allow running as root, which is not recommended",
    "\nArguments:",
    "run <executable>        Runs the specified executable"
};


enum class RunMode {
    None,
    Run,
    Destroy
};

struct CommandLine {
    RunMode mode = RunMode::None;
    bool runImmediately = false;
    bool allowRoot = false;
    bool handled = false;
    std::vector<std::string> target;
};

std::filesystem::path installDir(const char *pArgv0) {
    std::error_code error;
    const std::filesystem::path self =
        std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error && !self.empty()) {
        return self.parent_path();
    }
    return std::filesystem::absolute(std::filesystem::path(pArgv0).parent_path(), error);
}

void printHelp() {
    for (const std::string& line : HelpOutput) {
        std::cout << line << std::endl;
    }
}

CommandLine parseCommandLine(int argc, char *argv[]) {
    CommandLine parsed;

    int index = 1;
    for (; index < argc; index++) {
        const std::string_view argument = argv[index];

        if (argument == "--help") {
            printHelp();
            parsed.handled = true;
            return parsed;
        }

        if (argument == "--version") {
            std::cout << tuxblox::reportedVersion(TuxBloxVersion) << std::endl;
            parsed.handled = true;
            return parsed;
        }

        // The release is what --version answers; this is the layer's own number.
        if (argument == "--component-version") {
            std::cout << TuxBloxVersion << std::endl;
            parsed.handled = true;
            return parsed;
        }

        if (argument == "--immediate") {
            parsed.runImmediately = true;
        } else if (argument == "--verify-integrity") {
            // The integrity check is no longer optional, but the flag is still accepted: an unknown argument ends the option list and becomes the executable to launch, so a launcher that still passes this would otherwise be told to run it.
        } else if (argument == "--allow-root") {
            parsed.allowRoot = true;
        } else if (argument == "--destroy") {
            parsed.mode = RunMode::Destroy;
        } else if (argument == "run") {
            parsed.mode = RunMode::Run;
        } else if (argument == "runinprefix") {
            tuxblox::log("The \"runinprefix\" option is deprecated, use \"run --immediate\" instead.");
            parsed.mode = RunMode::Run;
            parsed.runImmediately = true;
        } else {
            break;
        }
    }

    for (; index < argc; index++) {
        parsed.target.push_back(argv[index]);
    }
    return parsed;
}

} // namespace

int runMain(int argc, char *argv[]) {
    const CommandLine command = parseCommandLine(argc, argv);
    if (command.handled) {
        return 0;
    }

    // Blocked from the start, so a stop that arrives while the virtual drive is
    // still being set up waits for the loop that knows how to act on it instead
    // of ending this process where it stands.
    sigset_t stopSignal;
    sigemptyset(&stopSignal);
    sigaddset(&stopSignal, SIGUSR1);
    sigprocmask(SIG_BLOCK, &stopSignal, nullptr);

    // After --help and --version, which the launcher asks of every binary on startup and must keep answering whatever user runs them.
    if (::geteuid() == 0) {
        if (!command.allowRoot) {
            tuxblox::log("Refusing to run as root. Re-run as a normal user, or pass --allow-root if you have a specific reason.");
            return 1;
        }
        tuxblox::log("WARNING: The current user is root, it is highly recommended to launch TuxBlox as a normal user unless there is a specific reason why");
    }

    if (command.mode == RunMode::None) {
        tuxblox::log("An option must be specified. Run --help for more details.");
        return 1;
    }

    if (command.mode == RunMode::Destroy && !command.target.empty()) {
        tuxblox::log("\"--destroy\" takes no arguments.");
        return 1;
    }

    if (command.mode == RunMode::Run && command.target.empty()) {
        tuxblox::log("\"run\" needs an executable to launch.");
        return 1;
    }

    const char *pPrefixDir = std::getenv("TUXBLOX_PREFIX");
    if (pPrefixDir == nullptr || *pPrefixDir == '\0') {
        tuxblox::log("TUXBLOX_PREFIX is not set?");
        return 1;
    }

    const std::string invocation = tuxblox::joinCommandLine({argv, argv + argc});

    // From the binary's own stamp: staging writes the release into every component, so each one says
    // which release it is part of without reading anything beside it.
    const std::string& release = tuxblox::releaseVersion();
    tuxblox::log("Runtime version " + TuxBloxVersion +
                 (release.empty() ? "" : " (release " + release + ")"));
    // Named up front because it is what ties a line in this log to the processes it started: every
    // one of them carries it, and it is how a session tells its own Roblox from one beside it.
    tuxblox::log("Session ID is " + tuxblox::currentSessionId());

    // Started here so the answer is on its way while the prefix and the executable are
    // resolved. Only a later flag read can wait, and a run the launcher did not start --
    // which is every `main run something.exe` by hand -- asks nobody at all.
    // parent_path(), like recordedRelease() above: installDir() is the folder holding
    // this binary, which is <root>/compat, and flags.json lives beside settings.json.
    tuxblox::initLayerFlags(installDir(argv[0]).parent_path().string());

    // The Roblox client's signature is checked by the compatibility layer itself, as the process
    // starts, rather than here. Here only covered a launch that came through this program, and the
    // layer can be reached without it: by running a loader directly, or by loading ntdll and
    // calling into it. Checking where every launch arrives covers all of them.

    tuxblox::Proton proton(installDir(argv[0]));
    proton.cleanupLegacyDist();

    const std::filesystem::path prefixDir = std::filesystem::path(pPrefixDir) / "pfx";

    tuxblox::Session session(proton, prefixDir);
    session.buildVersion = TuxBloxVersion;
    session.invocation = invocation;
    session.layerBinary = installDir(argv[0]) / "main";
    session.tuxbloxPrefix = pPrefixDir;
    session.initWine();

    tuxblox::Prefix prefix(proton, std::filesystem::path(pPrefixDir), TuxBloxVersion);

    if (command.mode == RunMode::Destroy) {
        prefix.removeTrackedFiles();
        return 0;
    }

    if (proton.missingDefaultPrefix()) {
        proton.makeDefaultPrefix(session.env,
                                 [&session](const std::vector<std::string>& command,
                                            const tuxblox::Environment& env) {
                                     return session.runProc(command, env);
                                 });
    }

    session.initSession();

    std::vector<std::string> target = command.target;

    // Before the drive is set up and before anything is launched: Roblox runs
    // one Player session at a time, and a new Player that finds the old one
    // still running closes itself within 50 ms instead of starting. Studio is
    // untouched -- several Studio sessions are expected to be open at once.
    if (command.mode == RunMode::Run && !target.empty()) {
        tuxblox::replaceRunningSession(prefixDir, tuxblox::imageNameOf(target.front()),
                                       installDir(argv[0]) / "main", pPrefixDir);
    }

    if (!command.runImmediately) {
        // Marked in use with no session left means the last run was cut off mid-write rather than
        // closed, so TuxBlox's own files in the drive are rebuilt before anything starts. Installed
        // Roblox versions, logs, shortcuts and documents are kept.
        if (tuxblox::driveNeedsRepair(pPrefixDir, tuxblox::otherSessionsInDrive(
                                                      session.layerBinary, session.tuxbloxPrefix))) {
            tuxblox::log("The virtual drive was not closed properly last time, so it is being repaired first.");
            prefix.removeTrackedFiles(tuxblox::Prefix::Removal::KeepAccumulatedState);
        }

        prefix.setup(session);

        // setup() may have just renamed the drive's account folder, and whoever asked for this launch worked the path out before that happened.
        const std::string account = tuxblox::prefixAccountName(prefixDir, tuxblox::accountName());
        for (std::string& entry : target) {
            entry = tuxblox::retargetAfterAccountRename(entry, prefixDir, account);
        }
    }

    session.applyDllOverrides();

    tuxblox::markDriveInUse(pPrefixDir);

    const int rc = session.run(target);

    // Every session waits here, whether or not it has company: Roblox relaunches itself, so the
    // process waited on above is not always the one still running, and a session that left at this
    // point would go on being reported as running with nothing left watching it. Only the last
    // session out goes on to close the drive, because whoever closes after it will be the last out.
    //
    // A stop that arrives while the drive is draining ends the run as a success: the user asked for it, so it is not a failure to report.
    if (session.waitForPrefixDrain(tuxblox::PrefixDrainTimeoutSeconds)) {
        return 0;
    }

    // TuxBlox exit codes:
    //   0 - OK (successful)
    //   1 - TuxBlox error
    //   2 - Launched process error
    // A refused signature arrives as the launched process's own exit code, since the layer is
    // what refuses it; the launcher reads that out of the log.
    return rc == 0 ? 0 : 2;
}

int main(int argc, char *argv[]) {
    // Before anything is written, so the log this process and everything it
    // launches shares cannot outgrow what a person can open and send. Does
    // nothing when the output is a terminal rather than a launcher's log file.
    tuxblox::watchLogFile(STDERR_FILENO);

    try {
        return runMain(argc, argv);
    } catch (const std::exception& failure) {
        tuxblox::log(std::string("TuxBlox could not start: ") + failure.what());
        return 1;
    }
}
