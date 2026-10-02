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

#include "process_name.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

namespace {

// What `ps`, `pgrep -x` and a system monitor actually read for this process.
std::string commFromProc() {
    std::ifstream file("/proc/self/comm");
    std::string name;
    std::getline(file, name);
    return name;
}

} // namespace

int main() {
    using namespace tuxblox;

    const std::string original = currentProcessName();
    assert(!original.empty());

    // The watcher's own name. It must reach the place the system lists processes from, or a `pgrep -x`
    // for the launcher would still match the watcher -- which is the whole point of renaming it.
    {
        assert(setProcessName("tuxbloxWatcher"));
        assert(currentProcessName() == "tuxbloxWatcher");
        assert(commFromProc() == "tuxbloxWatcher");
    }

    // Linux keeps this in 16 bytes, so a longer name is truncated rather than refused. Truncating here
    // rather than letting the kernel do it silently keeps what was asked for and what was set the same.
    {
        assert(setProcessName("averyverylongprocessname"));
        const std::string set = currentProcessName();
        assert(set.size() == 15);
        assert(set == "averyverylongpr");
        assert(commFromProc() == set);
    }

    // A name of exactly the limit survives whole.
    {
        assert(setProcessName("123456789012345"));
        assert(currentProcessName() == "123456789012345");
    }

    // Nothing to set is refused, and leaves the name alone rather than blanking it.
    {
        const std::string before = currentProcessName();
        assert(!setProcessName(""));
        assert(currentProcessName() == before);
    }

    setProcessName(original);
    printf("process_name: all tests passed\n");
    return 0;
}
