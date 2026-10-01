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

#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

namespace tuxblox {

// Puts a message on the desktop through the standard notification service,
// the same way the compatibility layer reports Roblox's own error dialogs.
// Best effort and never blocking: it returns as soon as the notifier has been
// started, and does nothing at all on a desktop without one.
void showDesktopNotification(const std::string& title, const std::string& body,
                             const std::string& icon = "dialog-error");

// The notify-send command line, program name first. Exposed for testing.
std::vector<std::string> notifySendArguments(const std::string& title, const std::string& body,
                                             const std::string& icon);

// The same command line with one action button, whose name is "update" and which notify-send prints when it is pressed. Exposed for testing.
std::vector<std::string> notifySendActionArguments(const std::string& title, const std::string& body,
                                                   const std::string& icon, const std::string& actionLabel);

// A notification with a button on it. show() returns at once; onAction runs on another thread if the button is pressed, so it must be safe to call from there. A notify-send too old for buttons falls back to the plain notification.
class ActionNotification {
public:
    ActionNotification() = default;
    ~ActionNotification() { cancel(); }
    ActionNotification(const ActionNotification&) = delete;
    ActionNotification& operator=(const ActionNotification&) = delete;

    void show(const std::string& title, const std::string& body, const std::string& icon,
              const std::string& actionLabel, std::function<void()> onAction);

    // Withdraws the notification and waits for its thread, so onAction can no longer run once this has returned
    void cancel();

private:
    pid_t pid_ = -1;
    std::atomic<bool> pidLive_{false};
    std::atomic<bool> cancelled_{false};
    std::thread thread_;
};

} // namespace tuxblox
