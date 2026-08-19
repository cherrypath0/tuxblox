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

#include "install_paths.h"
#include <cstdlib>
#include <stdexcept>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

std::string installDir() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
        throw std::runtime_error("installDir: HOME environment variable is not set");
    }
    return std::string(home) + "/.tuxblox";
}

bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes) {
    struct statvfs st{};
    if (statvfs(path.c_str(), &st) != 0) {
        throw std::runtime_error("hasEnoughDiskSpace: statvfs failed for " + path);
    }
    uint64_t freeBytes = static_cast<uint64_t>(st.f_bavail) * static_cast<uint64_t>(st.f_frsize);
    return freeBytes >= minBytes;
}

std::string protonDirUnder(const std::string& installDir) {
    return installDir + "/proton";
}

std::optional<std::string> readInstalledProtonVersion(const std::string& installDir) {
    const std::string bin = protonDirUnder(installDir) + "/main";
    if (access(bin.c_str(), X_OK) != 0) return std::nullopt;

    // fork/exec instead of popen: no shell involved, so installDir needs no
    // quoting/escaping.
    int fds[2];
    if (pipe(fds) != 0) return std::nullopt;
    const pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return std::nullopt;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execl(bin.c_str(), bin.c_str(), "--version", static_cast<char*>(nullptr));
        _exit(127);
    }
    close(fds[1]);

    std::string out;
    char buf[256];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0) out.append(buf, static_cast<size_t>(n));
    close(fds[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return std::nullopt;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return std::nullopt;

    out = out.substr(0, out.find('\n'));
    while (!out.empty() && (out.back() == '\r' || out.back() == ' ' || out.back() == '\t')) out.pop_back();
    if (out.empty()) return std::nullopt;
    return out;
}

std::string selfExePath() {
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf);
}

} // namespace tuxblox
