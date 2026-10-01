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
#include "child_environment.h"

#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

std::vector<std::string> notifySendArguments(const std::string& title, const std::string& body,
                                             const std::string& icon) {
    return {"notify-send", "-a", "TuxBlox", "-i", icon, "--", title, body};
}

std::vector<std::string> notifySendActionArguments(const std::string& title, const std::string& body,
                                                   const std::string& icon, const std::string& actionLabel) {
    return {"notify-send", "-a", "TuxBlox", "-i", icon, "--action=update=" + actionLabel, "--", title, body};
}

void showDesktopNotification(const std::string& title, const std::string& body, const std::string& icon) {
    std::vector<std::string> arguments = notifySendArguments(title, body, icon);
    std::vector<char*> argv;
    for (std::string& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    // notify-send is the desktop's own program, so it must not inherit the interface libraries' settings paths
    ChildEnvironment environment;

    const pid_t pid = fork();
    if (pid < 0) return;

    if (pid == 0) {
        // Forked twice so the notifier belongs to init rather than to us. The
        // callers here go on to exec into Roblox and never wait for anyone, so
        // a single fork would leave the notifier reparented mid-flight or, on
        // the watched path, sitting as a zombie for the whole session.
        if (fork() == 0) {
            execvpe("notify-send", argv.data(), environment.envp());
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

void ActionNotification::show(const std::string& title, const std::string& body, const std::string& icon,
                              const std::string& actionLabel, std::function<void()> onAction) {
    cancel();
    cancelled_.store(false);

    std::vector<std::string> arguments = notifySendActionArguments(title, body, icon, actionLabel);
    std::vector<char*> argv;
    for (std::string& argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);
    ChildEnvironment environment;

    int pipeEnds[2];
    // Close-on-exec, or Wine and the browser started next would inherit the read end for as long as the notice is up
    if (pipe2(pipeEnds, O_CLOEXEC) != 0) return;
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipeEnds[0]);
        close(pipeEnds[1]);
        return;
    }
    if (pid == 0) {
        close(pipeEnds[0]);
        dup2(pipeEnds[1], STDOUT_FILENO);
        close(pipeEnds[1]);
        execvpe("notify-send", argv.data(), environment.envp());
        _exit(127);
    }
    close(pipeEnds[1]);
    pid_ = pid;
    pidLive_.store(true);

    const int readFd = pipeEnds[0];
    thread_ = std::thread([this, readFd, pid, title, body, icon, onAction = std::move(onAction)] {
        std::string output;
        char buffer[128];
        ssize_t count = 0;
        while ((count = read(readFd, buffer, sizeof(buffer))) > 0) output.append(buffer, static_cast<size_t>(count));
        close(readFd);
        int status = 0;
        waitpid(pid, &status, 0);
        pidLive_.store(false);
        if (cancelled_.load()) return;

        if (output.find("update") != std::string::npos) {
            onAction();
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            // Failing at once with nothing printed means this notify-send has no buttons (or no notification service at all), so the notice goes out the plain way
            showDesktopNotification(title, body, icon);
        }
    });
}

void ActionNotification::cancel() {
    cancelled_.store(true);
    // notify-send takes its notification down on an interrupt, and a plain terminate would leave it on screen with a button that does nothing
    if (pidLive_.load() && pid_ > 0) kill(pid_, SIGINT);
    if (thread_.joinable()) thread_.join();
    pid_ = -1;
}

} // namespace tuxblox
