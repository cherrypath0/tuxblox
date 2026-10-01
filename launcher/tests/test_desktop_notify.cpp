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

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

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

    printf("desktop_notify: all tests passed\n");
    return 0;
}
