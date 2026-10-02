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

#include "support/sessions.h"

#include "support/util.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string_view>

#include <time.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// Only these keep a drive's session alive. The crash handler, StudioMCP and
// RCCService are helpers: if they are all that is left, the session is over.
const std::array<std::string_view, 2> ClientHolderImages = {
    "robloxplayerbeta.exe",
    "robloxstudiobeta.exe"
};

const std::array<std::string_view, 2> InstallerHolderImages = {
    "robloxplayerinstaller.exe",
    "robloxstudioinstaller.exe"
};

// How long a session being replaced is given to close before it is insisted on.
const int ReplaceWaitSeconds = 10;

std::string toLower(const std::string& text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

std::string readFirstEntry(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return "";
    }
    std::string first;
    std::getline(in, first, '\0');
    return first;
}

// The image name in a command line, cut at the first ".exe" rather than the
// first space: the image path can contain spaces, and later arguments can
// contain further ".exe" paths.
std::string imageInCommandLine(const std::string& commandLine) {
    const size_t cut = toLower(commandLine).find(".exe");
    if (cut == std::string::npos) {
        return "";
    }
    return imageNameOf(commandLine.substr(0, cut + 4));
}

// A Wine process's /proc/<pid>/cmdline holds the Windows command line, so the
// image name is what identifies it. comm is no use: Studio names its main
// thread "Main".
std::string pidWineImage(const fs::path& procRoot, const std::string& pid) {
    return imageInCommandLine(readFirstEntry(procRoot / pid / "cmdline"));
}

// The Roblox executable a compatibility-layer process was told to run. Its own
// path holds no ".exe", so the first one in the whole command line is the
// target -- reading only the first entry, as pidWineImage does, finds nothing.
std::string pidLayerTarget(const fs::path& procRoot, const std::string& pid) {
    std::ifstream in(procRoot / pid / "cmdline", std::ios::binary);
    if (!in) {
        return "";
    }
    std::string joined((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (char& c : joined) {
        if (c == '\0') c = ' ';
    }
    return imageInCommandLine(joined);
}

std::string envValue(const fs::path& environPath, const std::string& key) {
    std::ifstream in(environPath, std::ios::binary);
    if (!in) {
        // Not ours to read (another user, a kernel thread), so not ours.
        return "";
    }
    std::string entry;
    const std::string wanted = key + "=";
    while (std::getline(in, entry, '\0')) {
        if (entry.compare(0, wanted.size(), wanted) == 0) {
            return entry.substr(wanted.size());
        }
    }
    return "";
}

// A trailing separator is stripped so ".../pfx/" and ".../pfx" compare equal:
// the launch sets WINEPREFIX with one, and the launcher's copy of this rule
// strips it, so leaving it on makes one of the two comparisons never match.
std::string normalized(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    std::string value = fs::path(path).lexically_normal().string();
    while (value.size() > 1 && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

bool isPidName(const std::string& name) {
    return !name.empty() && std::all_of(name.begin(), name.end(),
                                        [](unsigned char c) { return std::isdigit(c); });
}

bool pidAlive(const std::string& pid) {
    return ::kill(std::atoi(pid.c_str()), 0) == 0;
}

} // namespace

std::string imageNameOf(const std::string& path) {
    std::string name = path;
    std::replace(name.begin(), name.end(), '\\', '/');
    const size_t slash = name.rfind('/');
    return slash == std::string::npos ? name : name.substr(slash + 1);
}

bool imageIsClient(const std::string& image) {
    const std::string lowered = toLower(image);
    return std::find(ClientHolderImages.begin(), ClientHolderImages.end(), lowered) !=
           ClientHolderImages.end();
}

bool imageIsInstaller(const std::string& image) {
    const std::string lowered = toLower(image);
    return std::find(InstallerHolderImages.begin(), InstallerHolderImages.end(), lowered) !=
           InstallerHolderImages.end();
}

std::vector<SessionHolder> prefixSessionHoldersIn(const fs::path& procRoot, const fs::path& prefixDir) {
    std::vector<SessionHolder> holders;
    const std::string wanted = normalized(prefixDir.string());

    std::error_code error;
    fs::directory_iterator procEntries(procRoot, error);
    if (error) {
        return holders;
    }

    for (const fs::directory_entry& entry : procEntries) {
        const std::string pid = entry.path().filename().string();
        if (!isPidName(pid)) {
            continue;
        }
        // Image name first: cmdline is world-readable and cheap, and it narrows
        // a few hundred processes down to the handful worth reading environ for.
        const std::string image = pidWineImage(procRoot, pid);
        const bool client = imageIsClient(image);
        if (!client && !imageIsInstaller(image)) {
            continue;
        }
        if (normalized(envValue(entry.path() / "environ", "WINEPREFIX")) != wanted) {
            continue;
        }
        SessionHolder holder;
        holder.pid = pid;
        holder.image = image;
        holder.client = client;
        holders.push_back(holder);
    }
    return holders;
}

std::vector<SessionHolder> prefixSessionHolders(const fs::path& prefixDir) {
    return prefixSessionHoldersIn("/proc", prefixDir);
}

std::vector<std::string> sessionsToReplace(const std::string& image,
                                           const std::vector<SessionHolder>& holders) {
    std::vector<std::string> pids;
    if (toLower(imageNameOf(image)) != ClientHolderImages[0]) {
        return pids;
    }
    for (const SessionHolder& holder : holders) {
        if (holder.client && toLower(holder.image) == ClientHolderImages[0]) {
            pids.push_back(holder.pid);
        }
    }
    return pids;
}

bool shouldTearDownPrefix(bool ownsPrefix, const std::vector<SessionHolder>& remaining) {
    return ownsPrefix && remaining.empty();
}

std::vector<std::string> otherLayerProcesses(const fs::path& procRoot, const fs::path& layerBinary,
                                             const std::string& tuxbloxPrefix,
                                             const std::string& image, int selfPid) {
    std::vector<std::string> pids;
    const fs::path wantedBinary = layerBinary.lexically_normal();
    const std::string wantedPrefix = normalized(tuxbloxPrefix);
    const std::string wantedImage = toLower(imageNameOf(image));

    std::error_code error;
    fs::directory_iterator procEntries(procRoot, error);
    if (error) {
        return pids;
    }

    for (const fs::directory_entry& entry : procEntries) {
        const std::string pid = entry.path().filename().string();
        if (!isPidName(pid) || std::atoi(pid.c_str()) == selfPid) {
            continue;
        }
        std::error_code linkError;
        const fs::path exe = fs::read_symlink(entry.path() / "exe", linkError);
        if (linkError || exe.lexically_normal() != wantedBinary) {
            continue;
        }
        if (toLower(pidLayerTarget(procRoot, pid)) != wantedImage) {
            continue;
        }
        if (normalized(envValue(entry.path() / "environ", "TUXBLOX_PREFIX")) != wantedPrefix) {
            continue;
        }
        pids.push_back(pid);
    }
    return pids;
}

void replaceRunningSession(const fs::path& prefixDir, const std::string& image,
                           const fs::path& layerBinary, const std::string& tuxbloxPrefix) {
    const std::vector<std::string> replacing =
        sessionsToReplace(image, prefixSessionHolders(prefixDir));
    if (replacing.empty()) {
        return;
    }

    log("Closing the Roblox Player session already running, so this one can take its place.");

    // Asking the other compatibility-layer process first means its session ends
    // the way a stop does, reporting success rather than a crash.
    const std::vector<std::string> owners = otherLayerProcesses(
        "/proc", layerBinary, tuxbloxPrefix, image, static_cast<int>(::getpid()));
    for (const std::string& owner : owners) {
        ::kill(std::atoi(owner.c_str()), SIGUSR1);
    }
    if (owners.empty()) {
        for (const std::string& pid : replacing) {
            ::kill(std::atoi(pid.c_str()), SIGTERM);
        }
    }

    const struct timespec pollInterval = {0, 100 * 1000 * 1000};
    for (int waited = 0; waited < ReplaceWaitSeconds * 10; waited++) {
        bool anyAlive = false;
        for (const std::string& pid : replacing) {
            if (pidAlive(pid)) {
                anyAlive = true;
                break;
            }
        }
        if (!anyAlive) {
            return;
        }
        ::nanosleep(&pollInterval, nullptr);
    }

    for (const std::string& pid : replacing) {
        if (pidAlive(pid)) {
            ::kill(std::atoi(pid.c_str()), SIGKILL);
        }
    }
    // Launching anyway rather than refusing: the new session will close itself
    // if the old one is somehow still there, which leaves the user with the
    // session they already had instead of with nothing.
    log("The previous session did not close in time, so this launch may not start.");
}

} // namespace tuxblox
