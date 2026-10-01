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

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Stands in for notify-send: the script decides what it prints and how it exits, and everything it was given is kept for the test to read
void writeFakeNotifySend(const fs::path &dir, const std::string &body) {
    fs::create_directories(dir);
    const fs::path script = dir / "notify-send";
    std::ofstream(script) << "#!/bin/sh\nprintf '%s\\n' \"$@\" >> \"$FAKE_NOTIFY_LOG\"\n" << body << "\n";
    chmod(script.c_str(), 0755);
}

bool waitUntil(const std::function<bool()> &condition) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (condition()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return condition();
}

std::string readAll(const fs::path &path) {
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace

int main() {
    using tuxblox::notifySendArguments;

    // Update notices carry the TuxBlox icon, while error notices keep the red one they always had
    const std::vector<std::string> update = notifySendArguments("TuxBlox", "TuxBlox 2.7.3 is available", "tuxblox");
    const std::vector<std::string> expectedUpdate = {"notify-send", "-a", "TuxBlox", "-i", "tuxblox", "--",
                                                     "TuxBlox", "TuxBlox 2.7.3 is available"};
    assert(update == expectedUpdate);

    const std::vector<std::string> errorNotice = notifySendArguments("Roblox", "It crashed", "dialog-error");
    assert(errorNotice[4] == "dialog-error");

    // The title and body come after "--", so a message starting with a dash is never read as an option by notify-send
    const std::vector<std::string> dashed = notifySendArguments("-t", "-u critical", "tuxblox");
    assert(dashed[5] == "--");
    assert(dashed[6] == "-t");
    assert(dashed[7] == "-u critical");

    const std::vector<std::string> withAction = tuxblox::notifySendActionArguments("TuxBlox", "TuxBlox 2.7.3 is available", "tuxblox", "Update");
    const std::vector<std::string> expectedWithAction = {"notify-send", "-a", "TuxBlox", "-i", "tuxblox", "--action=update=Update", "--",
                                                         "TuxBlox", "TuxBlox 2.7.3 is available"};
    assert(withAction == expectedWithAction);

    const fs::path work = fs::temp_directory_path() / ("desktop_notify_test_" + std::to_string(getpid()));
    fs::remove_all(work);
    fs::create_directories(work);
    const std::string oldPath = getenv("PATH") ? getenv("PATH") : "";
    setenv("FAKE_NOTIFY_LOG", (work / "log").c_str(), 1);

    // Pressing the button prints its name, and that is what runs the action
    {
        writeFakeNotifySend(work / "pressed", "printf 'update\\n'");
        setenv("PATH", ((work / "pressed").string() + ":" + oldPath).c_str(), 1);
        std::atomic<int> pressedCount{0};
        tuxblox::ActionNotification notice;
        notice.show("TuxBlox", "body", "tuxblox", "Update", [&] { ++pressedCount; });
        assert(waitUntil([&] { return pressedCount.load() == 1; }));
        notice.cancel();
        assert(pressedCount.load() == 1);
        assert(readAll(work / "log").find("--action=update=Update") != std::string::npos);
    }

    // Closing the notice without pressing anything prints nothing, and nothing runs
    {
        fs::remove(work / "log");
        writeFakeNotifySend(work / "dismissed", "exit 0");
        setenv("PATH", ((work / "dismissed").string() + ":" + oldPath).c_str(), 1);
        std::atomic<int> pressedCount{0};
        tuxblox::ActionNotification notice;
        notice.show("TuxBlox", "body", "tuxblox", "Update", [&] { ++pressedCount; });
        assert(waitUntil([&] { return fs::exists(work / "log"); }));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        notice.cancel();
        assert(pressedCount.load() == 0);
    }

    // A notify-send too old to know --action fails at once, and the notice is still shown, just without the button
    {
        fs::remove(work / "log");
        writeFakeNotifySend(work / "old", "case \"$*\" in *--action*) exit 1;; esac");
        setenv("PATH", ((work / "old").string() + ":" + oldPath).c_str(), 1);
        std::atomic<int> pressedCount{0};
        tuxblox::ActionNotification notice;
        notice.show("TuxBlox", "plain body", "tuxblox", "Update", [&] { ++pressedCount; });
        assert(waitUntil([&] { return readAll(work / "log").find("plain body") != std::string::npos &&
                                      readAll(work / "log").rfind("plain body") != readAll(work / "log").find("plain body"); }));
        notice.cancel();
        assert(pressedCount.load() == 0);
    }

    // Closing the launcher must not leave the notice or its thread waiting for a click that can no longer be acted on
    {
        writeFakeNotifySend(work / "waiting", "sleep 30");
        setenv("PATH", ((work / "waiting").string() + ":" + oldPath).c_str(), 1);
        tuxblox::ActionNotification notice;
        notice.show("TuxBlox", "body", "tuxblox", "Update", [] {});
        const auto start = std::chrono::steady_clock::now();
        notice.cancel();
        assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
    }

    setenv("PATH", oldPath.c_str(), 1);
    fs::remove_all(work);
    printf("desktop_notify: all tests passed\n");
    return 0;
}
