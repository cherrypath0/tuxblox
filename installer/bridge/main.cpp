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

#include "bridge.h"

#include "checksum.h"
#include "downloader.h"
#include "version.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

using namespace tuxblox;

namespace {

// Removed on every exit but a successful exec, which replaces this process and keeps the file.
struct ScratchDir {
    std::string path;
    ~ScratchDir() {
        if (!path.empty()) {
            unlink((path + "/TuxBloxInstaller").c_str());
            rmdir(path.c_str());
        }
    }
};

std::string makeScratchDir() {
    char pattern[] = "/tmp/tuxblox-bridge-XXXXXX";
    return mkdtemp(pattern) == nullptr ? std::string() : std::string(pattern);
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    if (bridge::wantsVersion(args)) {
        printf("%s\n", kTuxBloxVersion);
        return 0;
    }
    if (bridge::answersItself(args)) {
        printf("TuxBlox bridge installer: fetches and runs the installer from release %s.\n",
               bridge::pinnedRelease());
        return 0;
    }

    ScratchDir scratch{makeScratchDir()};
    if (scratch.path.empty()) {
        fprintf(stderr, "bridge: could not make a temporary directory. Nothing has been changed.\n");
        return 1;
    }
    const std::string installer = scratch.path + "/TuxBloxInstaller";

    printf(">>> TuxBlox is moving to its new update layout. Fetching the installer from %s.\n",
           bridge::pinnedRelease());
    fflush(stdout);

    DownloadOutcome outcome{DownloadResult::Failed, "no address was tried"};
    for (const std::string& url : bridge::installerUrls()) {
        outcome = downloadFile(url, installer, [](uint64_t, uint64_t) {}, nullptr);
        if (outcome.result == DownloadResult::Ok) break;
        fprintf(stderr, "bridge: %s did not answer (%s), trying the next address\n",
                url.c_str(), outcome.errorMessage.c_str());
    }
    if (outcome.result != DownloadResult::Ok) {
        fprintf(stderr, "bridge: could not download the installer. Nothing has been changed; try "
                        "again later.\n");
        return 1;
    }

    std::string got;
    try {
        got = sha256File(installer);
    } catch (const std::exception& e) {
        fprintf(stderr, "bridge: could not read the installer back (%s). Nothing has been "
                        "changed.\n", e.what());
        return 1;
    }
    if (got != bridge::installerSha256()) {
        fprintf(stderr, "bridge: the downloaded installer does not match its expected checksum. "
                        "Nothing has been changed.\n");
        return 1;
    }

    if (chmod(installer.c_str(), 0755) != 0) {
        fprintf(stderr, "bridge: could not make the installer executable. Nothing has been "
                        "changed.\n");
        return 1;
    }

    // Everything the caller passed, unchanged: a --release-file names the release the launcher
    // actually resolved, and deciding one here would put every caller on the pinned release.
    std::vector<char*> forwarded{const_cast<char*>(installer.c_str())};
    for (int at = 1; at < argc; ++at) forwarded.push_back(argv[at]);
    forwarded.push_back(nullptr);

    execv(installer.c_str(), forwarded.data());
    fprintf(stderr, "bridge: downloaded the installer but could not start it at %s\n",
            installer.c_str());
    return 1;
}
