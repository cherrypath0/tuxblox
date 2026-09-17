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

#include "desktop_notify.h"

#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

void showDesktopNotification(const std::string& title, const std::string& body) {
    const pid_t pid = fork();
    if (pid < 0) return;

    if (pid == 0) {
        // Forked twice so the notifier belongs to init rather than to us. The
        // callers here go on to exec into Roblox and never wait for anyone, so
        // a single fork would leave the notifier reparented mid-flight or, on
        // the watched path, sitting as a zombie for the whole session.
        if (fork() == 0) {
            execlp("notify-send", "notify-send", "-a", "TuxBlox",
                   "-i", "dialog-error", title.c_str(), body.c_str(),
                   static_cast<char*>(nullptr));
            // No notify-send on this desktop: nothing to report it to, and a
            // missing notification must never be worth failing a launch over.
            _exit(127);
        }
        _exit(0);
    }

    // Reaps the middle process only, which exits immediately.
    int status = 0;
    waitpid(pid, &status, 0);
}

} // namespace tuxblox
