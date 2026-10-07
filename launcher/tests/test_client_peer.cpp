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

#include "client_peer.h"

#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <set>
#include <string>

int main() {
    // /proc writes an address as four bytes of hex, least significant first.
    assert(tuxblox::ipv4FromProcHex("08080808") == "8.8.8.8");
    assert(tuxblox::ipv4FromProcHex("0201A8C0") == "192.168.1.2");
    assert(tuxblox::ipv4FromProcHex("211D7480") == "128.116.29.33");
    assert(tuxblox::ipv4FromProcHex("00000000") == "0.0.0.0");
    assert(tuxblox::ipv4FromProcHex("0100007F") == "127.0.0.1");
    assert(tuxblox::ipv4FromProcHex("zzzz").empty());
    assert(tuxblox::ipv4FromProcHex("0808").empty());
    assert(tuxblox::ipv4FromProcHex("").empty());

    // A real table, with the columns the kernel actually writes.
    const std::string table =
        "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode ref pointer drops\n"
        "  413: 0201A8C0:ADD1 211D7480:C13A 01 00000000:00000000 00:00000000 00000000  1000        0 21831389 2 0000000000000000 0\n"
        "  414: 0201A8C0:ADD2 0100007F:C13B 01 00000000:00000000 00:00000000 00000000  1000        0 21831390 2 0000000000000000 0\n"
        "  415: 0201A8C0:ADD3 00000000:0000 07 00000000:00000000 00:00000000 00000000  1000        0 21831391 2 0000000000000000 0\n"
        "  416: 0201A8C0:ADD4 08080808:0035 01 00000000:00000000 00:00000000 00000000  1000        0 99999999 2 0000000000000000 0\n";

    // Only sockets this process holds, and only addresses worth asking about.
    std::set<std::string> ours = {"21831389", "21831390", "21831391"};
    const std::vector<std::string> peers = tuxblox::routablePeersFromProcNet(table, ours);
    assert(peers.size() == 1);
    assert(peers.front() == "128.116.29.33");

    // A socket belonging to something else is not ours to report.
    std::set<std::string> none = {"12345"};
    assert(tuxblox::routablePeersFromProcNet(table, none).empty());

    // Nothing sensible in, nothing out, and never a throw.
    assert(tuxblox::routablePeersFromProcNet("", ours).empty());
    assert(tuxblox::routablePeersFromProcNet("garbage\nmore garbage\n", ours).empty());
    assert(tuxblox::routablePeersFromProcNet(table, {}).empty());

    // The inode a /proc/<pid>/fd entry points at.
    assert(tuxblox::socketInodeFromLink("socket:[21831389]") == "21831389");
    assert(tuxblox::socketInodeFromLink("/dev/null").empty());
    assert(tuxblox::socketInodeFromLink("socket:[]").empty());
    assert(tuxblox::socketInodeFromLink("socket:[abc]").empty());
    assert(tuxblox::socketInodeFromLink("").empty());

    // Which processes count. NOT the name the kernel reports: Roblox renames its own main thread, so
    // /proc/<pid>/comm reads "Main" for a running client -- measured live 2026-10-07. The command
    // line is what names the program, and its first argument is the exe that was started.
    // Arguments are separated by a zero byte, so they are joined here rather than counted by hand.
    const auto argv = [](std::initializer_list<const char *> parts) {
        std::string out;
        for (const char *pPart : parts) {
            out += pPart;
            out.push_back('\0');
        }
        return out;
    };

    // Exactly what a running client reports, copied from /proc on 2026-10-07.
    assert(tuxblox::cmdlineNamesRobloxClient(argv(
        {"/home/cherry/.tuxblox/runtime/pfx/drive_c/users/cherry/AppData/Local/Roblox/Versions/"
         "version-2366ba214ec740ca/RobloxPlayerBeta.exe"})));
    assert(tuxblox::cmdlineNamesRobloxClient(argv({"C:\\x\\RobloxStudioBeta.exe", "--args"})));
    // Only the first argument counts, or TuxBlox's own wrapper would answer for the client.
    assert(!tuxblox::cmdlineNamesRobloxClient(
        argv({"/home/cherry/.tuxblox/compat/main", "run", "/x/RobloxPlayerBeta.exe"})));
    assert(!tuxblox::cmdlineNamesRobloxClient(argv({"/usr/bin/wineserver"})));
    assert(!tuxblox::cmdlineNamesRobloxClient(std::string()));

    assert(tuxblox::environNamesPrefix(std::string("WINEPREFIX=/home/a/.tuxblox/runtime/pfx\0OTHER=1", 46),
                                       "/home/a/.tuxblox/runtime/pfx"));
    assert(!tuxblox::environNamesPrefix(std::string("WINEPREFIX=/somewhere/else/pfx\0X=1", 34),
                                       "/home/a/.tuxblox/runtime/pfx"));
    assert(!tuxblox::environNamesPrefix(std::string(), "/home/a/.tuxblox/runtime/pfx"));

    printf("client_peer: all tests passed\n");
    return 0;
}
