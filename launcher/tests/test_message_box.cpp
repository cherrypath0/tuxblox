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

#include "message_box.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

// With no display the message must still reach stderr, which the session log records, and the process must carry on with its environment as it was
int main() {
    const fs::path stack = fs::read_symlink("/proc/self/exe").parent_path() / "libtuxblox";
    fs::create_directories(stack / "fonts");
    fs::create_directories(stack / "share/glib-2.0/schemas");
    std::ofstream(stack / "fonts/fonts.conf") << "<fontconfig/>";
    std::ofstream(stack / "share/glib-2.0/schemas/gschemas.compiled") << "x";

    int pipeEnds[2];
    assert(pipe(pipeEnds) == 0);
    const pid_t pid = fork();
    if (pid == 0) {
        close(pipeEnds[0]);
        dup2(pipeEnds[1], STDERR_FILENO);
        // X11 with no DISPLAY cannot connect anywhere, whereas an unset WAYLAND_DISPLAY would still try wayland-0 on a desktop
        setenv("GDK_BACKEND", "x11", 1);
        // The bundled font settings make fontconfig write a cache, which must not land in the source tree
        setenv("XDG_CACHE_HOME", (fs::temp_directory_path() / "message_box_cache").c_str(), 1);
        unsetenv("DISPLAY");
        setenv("FONTCONFIG_FILE", "/host/fonts.conf", 1);
        tuxblox::showErrorMessageBox("Test title", "Test message body");
        // With nowhere to show the question, the optional action is never taken on the user's behalf
        if (tuxblox::showErrorMessageBoxWithAction("Action title", "Action body", "Report")) _exit(4);
        const char *pAfter = getenv("FONTCONFIG_FILE");
        _exit(pAfter != nullptr && std::string(pAfter) == "/host/fonts.conf" ? 0 : 3);
    }
    close(pipeEnds[1]);
    std::string output;
    char buffer[512];
    ssize_t count = 0;
    while ((count = read(pipeEnds[0], buffer, sizeof(buffer))) > 0) output.append(buffer, static_cast<size_t>(count));
    int status = 0;
    waitpid(pid, &status, 0);
    fs::remove_all(stack);

    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
    assert(output.find("Test title") != std::string::npos);
    assert(output.find("Test message body") != std::string::npos);
    assert(output.find("Action title") != std::string::npos);
    assert(output.find("Action body") != std::string::npos);
    printf("message_box: all tests passed\n");
    return 0;
}
