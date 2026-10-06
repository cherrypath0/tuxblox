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

#include "player_presence.h"

#include <cassert>
#include <cstdio>
#include <string>

// Roblox stamps every record it writes with the time, the uptime, the thread and the level before the
// channel. A game's print() can occupy a line of its own with no stamp at all, so the stamp is part of
// what makes a line Roblox's rather than a game's.
#define kRec "2026-10-06T20:31:49.889Z,11.889527,00d4,6 "

int main() {
    tuxblox::PlayerPresenceReader reader;
    assert(reader.activity().kind == tuxblox::PresenceKind::None);

    reader.consumeLine("2026-10-06T20:31:39.165Z,1.165060,00d0,6 [FLog::SessionTransitionFSM] Entered app session.");
    assert(reader.activity().kind == tuxblox::PresenceKind::InApp);

    reader.consumeLine("2026-10-06T20:31:49.889Z,11.889527,00d4,6 [FLog::Output] ! Joining game '5ccfb412-0b58-47d2-b91e-321903eceb59' place 6516141723 at 10.34.6.31");
    assert(reader.activity().kind == tuxblox::PresenceKind::Joining);
    assert(reader.activity().placeId == "6516141723");
    // The address on the join line is private in this session and public in others, so it is never used.
    assert(reader.serverAddress().empty());

    reader.consumeLine("2026-10-06T20:31:49.892Z,11.892516,00d4,6 [FLog::SessionTransitionFSM] Entered play session.");
    assert(reader.activity().kind == tuxblox::PresenceKind::Playing);

    reader.consumeLine("2026-10-06T20:31:50.122Z,12.122521,00d4,7 [FLog::Network] serverId: 128.116.31.33|61772");
    assert(reader.serverAddress() == "128.116.31.33");

    // A real teleport, captured 2026-10-06: disconnect, Teleported., the new join, then play again.
    reader.consumeLine("2026-10-06T20:31:59.248Z,21.248669,0190,7 [FLog::Network] Time to disconnect replication data: 0.401000");
    reader.consumeLine("2026-10-06T20:31:59.305Z,21.305782,0178,6 [FLog::SessionTransitionFSM] Teleported.");
    assert(reader.activity().kind == tuxblox::PresenceKind::Teleporting);
    reader.consumeLine("2026-10-06T20:31:59.316Z,21.316450,0178,6 [FLog::Output] ! Joining game '9600344a-517b-44c6-83be-5970b30f4f10' place 6839171747 at 75.45.133.91");
    assert(reader.activity().placeId == "6839171747");
    reader.consumeLine("2026-10-06T20:31:59.318Z,21.318447,00d4,6 [FLog::SessionTransitionFSM] Entered play session.");
    assert(reader.activity().kind == tuxblox::PresenceKind::Playing);
    reader.consumeLine("2026-10-06T20:31:59.725Z,21.725744,017c,7 [FLog::Network] serverId: 75.45.133.91|62152");
    assert(reader.serverAddress() == "75.45.133.91");

    // Leaving must not leave the last place showing.
    reader.consumeLine("2026-10-06T20:33:00.000Z,82.0,00d4,6 [FLog::SessionTransitionFSM] Tearing down");
    assert(reader.activity().kind == tuxblox::PresenceKind::InApp);
    assert(reader.activity().placeId.empty());

    // A local playtest has no place anybody can be sent to.
    tuxblox::PlayerPresenceReader local;
    local.consumeLine(kRec "[FLog::Output] ! Joining game '00000000-0000-0000-0000-000000000000' place 0 at localhost");
    assert(local.activity().placeId.empty());

    // What the game asked for rides along on the activity.
    tuxblox::PlayerPresenceReader rpc;
    rpc.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered play session.");
    rpc.consumeLine(kRec "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"Floor 3\"}}");
    assert(rpc.activity().rpc.details.value == "Floor 3");

    // Review Focus 2: a reader handed half a line must not act on it until the line is whole.
    tuxblox::PlayerPresenceReader split;
    split.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered pl");
    assert(split.activity().kind == tuxblox::PresenceKind::None);
    split.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered play session.");
    assert(split.activity().kind == tuxblox::PresenceKind::Playing);

    // A game's own print() lands on FLog::Output in the same log, so a line is only believed on the
    // channel it belongs to -- otherwise four print calls forge the whole session.
    tuxblox::PlayerPresenceReader forge;
    forge.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered play session.");
    forge.consumeLine("2026-10-06T20:31:50Z,12.1,00d4,6 [FLog::Output] [FLog::Network] serverId: 8.8.8.8|1234");
    assert(forge.serverAddress().empty());
    forge.consumeLine("2026-10-06T20:31:50Z,12.1,00d4,6 [FLog::Output] print says [FLog::SessionTransitionFSM] Tearing down");
    assert(forge.activity().kind == tuxblox::PresenceKind::Playing);

    // The genuine lines still work, on their own channels.
    forge.consumeLine("2026-10-06T20:31:50.122Z,12.122521,00d4,7 [FLog::Network] serverId: 128.116.31.33|61772");
    assert(forge.serverAddress() == "128.116.31.33");

    // A place id is bounded: a forged line carrying thousands of digits must not reach a request.
    tuxblox::PlayerPresenceReader huge;
    huge.consumeLine(std::string(kRec "[FLog::Output] ! Joining game 'x' place ") + std::string(4000, '9') + " at 1.1.1.1");
    assert(huge.activity().placeId.empty());

    // What a game asked to be shown must not follow the user out of that game.
    tuxblox::PlayerPresenceReader sticky;
    sticky.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered play session.");
    sticky.consumeLine(kRec "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"Floor 3\",\"state\":\"Hiding\"}}");
    assert(sticky.activity().rpc.details.value == "Floor 3");
    sticky.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered app session.");
    assert(sticky.activity().rpc.details.state == tuxblox::RpcField::State::Absent);
    assert(sticky.activity().rpc.state.state == tuxblox::RpcField::State::Absent);
    sticky.consumeLine(kRec "[FLog::Output] ! Joining game 'y' place 222 at 1.1.1.1");
    sticky.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered play session.");
    assert(sticky.activity().rpc.details.value.empty());

    // The server's own id is on the join line, and a join link cannot be built without it.
    assert(tuxblox::jobIdFromJoinLine(
               kRec "[FLog::Output] ! Joining game '5ccfb412-0b58-47d2-b91e-321903eceb59' place 6516141723 at 10.34.6.31") ==
           "5ccfb412-0b58-47d2-b91e-321903eceb59");
    // Studio's Team Create line carries it unquoted.
    assert(tuxblox::jobIdFromJoinLine(
               kRec "[FLog::TeamCreateJoinPayload] Joining game ef503732-2947-40fb-b5a4-e54976786c83 place 74031704898313 at [10.190.0.134]:62012") ==
           "ef503732-2947-40fb-b5a4-e54976786c83");
    // Anything that is not one is refused, so nothing a game writes can reach the link.
    assert(tuxblox::jobIdFromJoinLine(kRec "[FLog::Output] ! Joining game 'x' place 1 at 1.1.1.1").empty());
    assert(tuxblox::jobIdFromJoinLine(
               kRec "[FLog::Output] ! Joining game '../../evil&x=1&y=2&z=333333333333' place 1 at 1.1.1.1").empty());
    assert(tuxblox::jobIdFromJoinLine("nothing here").empty());

    tuxblox::PlayerPresenceReader ids;
    ids.consumeLine(kRec "[FLog::Output] ! Joining game '5ccfb412-0b58-47d2-b91e-321903eceb59' place 6516141723 at 10.34.6.31");
    // The join line alone is not enough: anybody can write one, so nothing is linked until Roblox confirms it.
    assert(ids.activity().jobId.empty());
    ids.consumeLine(kRec "[FLog::GameJoinLoadTime] Report game_join_loadtime: placeid:6516141723, join_time:0, universeid:2440500124,");
    assert(ids.activity().jobId == "5ccfb412-0b58-47d2-b91e-321903eceb59");
    // Leaving must not leave a stale server behind for a join link to point at.
    ids.consumeLine(kRec "[FLog::SessionTransitionFSM] Tearing down");
    assert(ids.activity().jobId.empty());

    // Roblox reports the place and experience it actually joined on a channel of its own, which a
    // game's print() cannot write to -- so that, not the join line, is what a clickable link is built from.
    assert(tuxblox::verifiedJoinFromLine(
               "2026-10-03T09:17:22.194Z,7.194335,00ec,6 [FLog::GameJoinLoadTime] Report game_join_loadtime: placeid:6516141723, join_time:0.62, universeid:2440500124, referral_page:")
               .placeId == "6516141723");
    assert(tuxblox::verifiedJoinFromLine(
               kRec "[FLog::GameJoinLoadTime] Report game_join_loadtime: placeid:6516141723, join_time:0, universeid:2440500124,")
               .universeId == "2440500124");
    // A game quoting the same text on its own channel is not Roblox reporting it.
    assert(tuxblox::verifiedJoinFromLine(
               kRec "[FLog::Output] [FLog::GameJoinLoadTime] Report game_join_loadtime: placeid:999, universeid:999,")
               .placeId.empty());
    assert(tuxblox::verifiedJoinFromLine(kRec "nothing").placeId.empty());

    // A forged join line gets no link at all, because nothing confirmed it.
    tuxblox::PlayerPresenceReader spoof;
    spoof.consumeLine(kRec "[FLog::Output] ! Joining game '5ccfb412-0b58-47d2-b91e-321903eceb59' place 1818 at 1.1.1.1");
    assert(spoof.activity().verifiedPlaceId.empty());
    assert(spoof.activity().jobId.empty());

    // The genuine pair is confirmed, so both the link and the experience are known.
    tuxblox::PlayerPresenceReader real;
    real.consumeLine(kRec "[FLog::Output] ! Joining game '5ccfb412-0b58-47d2-b91e-321903eceb59' place 6516141723 at 10.34.6.31");
    real.consumeLine(kRec "[FLog::GameJoinLoadTime] Report game_join_loadtime: placeid:6516141723, join_time:0, universeid:2440500124,");
    assert(real.activity().verifiedPlaceId == "6516141723");
    assert(real.activity().universeId == "2440500124");
    assert(real.activity().jobId == "5ccfb412-0b58-47d2-b91e-321903eceb59");

    // A game naming the real place but a server of its own choosing does not get that server linked.
    tuxblox::PlayerPresenceReader halfSpoof;
    halfSpoof.consumeLine(kRec "[FLog::Output] ! Joining game 'aaaaaaaa-0000-0000-0000-000000000000' place 999 at 1.1.1.1");
    halfSpoof.consumeLine(kRec "[FLog::GameJoinLoadTime] Report game_join_loadtime: placeid:6516141723, join_time:0, universeid:2440500124,");
    assert(halfSpoof.activity().verifiedPlaceId == "6516141723");
    assert(halfSpoof.activity().jobId.empty());

    // Leaving clears what was confirmed, so no link outlives the session.
    real.consumeLine(kRec "[FLog::SessionTransitionFSM] Tearing down");
    assert(real.activity().verifiedPlaceId.empty());
    assert(real.activity().universeId.empty());
    assert(real.activity().jobId.empty());

    // A game's output can take a line of its own with no stamp on it, by printing a newline. Such a
    // line claims whatever channel it likes, so it is not Roblox's record and nothing on it is believed.
    tuxblox::PlayerPresenceReader unstamped;
    unstamped.consumeLine(kRec "[FLog::SessionTransitionFSM] Entered play session.");
    unstamped.consumeLine("[FLog::Network] serverId: 8.8.8.8|1234");
    assert(unstamped.serverAddress().empty());
    unstamped.consumeLine("[FLog::SessionTransitionFSM] Tearing down");
    assert(unstamped.activity().kind == tuxblox::PresenceKind::Playing);
    assert(tuxblox::verifiedJoinFromLine(
               "[FLog::GameJoinLoadTime] Report game_join_loadtime: placeid:1818, universeid:13058,")
               .placeId.empty());

    // Roblox's own unstamped lines exist too -- its terms notice is one -- and are simply not records.
    assert(tuxblox::serverAddressFromLine(
               "[FLog::Output] All use of Roblox services must comply with Roblox's Terms of Use")
               .empty());

    printf("player_presence: all tests passed\n");
    return 0;
}
