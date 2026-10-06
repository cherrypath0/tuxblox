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


#include "bloxstrap_rpc.h"

#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using tuxblox::RpcField;

    assert(tuxblox::isBloxstrapRpcLine(
        "2026-10-06T20:31:49Z,11.8,00d4,6 [FLog::Output] [BloxstrapRPC] {\"command\": \"SetRichPresence\", \"data\": {\"details\": \"hi\"}}"));
    assert(!tuxblox::isBloxstrapRpcLine("[FLog::Output] ordinary game print"));

    tuxblox::RpcOverride o;

    // A value sets the field.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"Floor 3\",\"state\":\"Hiding\"}}", o));
    assert(o.details.state == RpcField::State::Set && o.details.value == "Floor 3");
    assert(o.state.state == RpcField::State::Set && o.state.value == "Hiding");

    // An absent field keeps what was there before.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"state\":\"Running\"}}", o));
    assert(o.details.value == "Floor 3");
    assert(o.state.value == "Running");

    // A blank string erases the field outright, and "<reset>" goes back to TuxBlox's own default.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"\",\"state\":\"<reset>\"}}", o));
    assert(o.details.state == RpcField::State::Clear);
    assert(o.state.state == RpcField::State::Reset);

    // Deprecated upstream but still sent by older games; zero erases.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"timeStart\":1700000000}}", o));
    assert(o.timeStart == 1700000000);
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"timeStart\":0}}", o));
    assert(o.timeStart == 0);

    // Images take a number or an rbxassetid string, and carry their own clear/reset flags.
    tuxblox::RpcOverride img;
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"largeImage\":{\"assetId\":1234,\"hoverText\":\"Map\"},\"smallImage\":{\"assetId\":\"rbxassetid://5678\"}}}", img));
    assert(img.largeImage.assetId.value == "1234");
    assert(img.largeImage.hoverText.value == "Map");
    assert(img.smallImage.assetId.value == "5678");
    // clear and reset are commands, not state: they act once, and a later assetId sets the image again.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"largeImage\":{\"clear\":true},\"smallImage\":{\"reset\":true}}}", img));
    assert(img.largeImage.assetId.state == RpcField::State::Clear);
    assert(img.largeImage.hoverText.state == RpcField::State::Clear);
    assert(img.smallImage.assetId.state == RpcField::State::Reset);

    // The ordinary pattern of leaving one stage and entering the next must keep working.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"largeImage\":{\"assetId\":222,\"hoverText\":\"Level 2\"}}}", img));
    assert(img.largeImage.assetId.state == RpcField::State::Set);
    assert(img.largeImage.assetId.value == "222");
    assert(img.largeImage.hoverText.value == "Level 2");

    // And a reset is just as recoverable, since no game ever sends reset:false.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"largeImage\":{\"reset\":true}}}", img));
    assert(img.largeImage.assetId.state == RpcField::State::Reset);
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"largeImage\":{\"assetId\":333}}}", img));
    assert(img.largeImage.assetId.value == "333");
    assert(img.largeImage.assetId.state == RpcField::State::Set);

    // A game must never be able to put an image of its own choosing on somebody's profile.
    assert(tuxblox::rpcAssetLookupUrl("1234") ==
           "https://thumbnails.roblox.com/v1/assets?assetIds=1234&size=512x512&format=Png");
    assert(tuxblox::rpcAssetLookupUrl("https://evil.example/pic.png").empty());
    assert(tuxblox::rpcAssetLookupUrl("12a4").empty());
    assert(tuxblox::rpcAssetLookupUrl("").empty());

    // Review Focus 4: valid JSON of the wrong shape, and anything oversized, is ignored and changes nothing.
    tuxblox::RpcOverride keep;
    keep.details.state = RpcField::State::Set;
    keep.details.value = "unchanged";
    const tuxblox::RpcOverride before = keep;
    assert(!tuxblox::applyBloxstrapRpcLine("[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":5}", keep));
    assert(!tuxblox::applyBloxstrapRpcLine("[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":null}", keep));
    assert(!tuxblox::applyBloxstrapRpcLine("[BloxstrapRPC] {\"command\":\"NotACommand\",\"data\":{\"details\":\"x\"}}", keep));
    assert(!tuxblox::applyBloxstrapRpcLine("[BloxstrapRPC] {\"command\":\"SetLaunchData\",\"data\":{\"data\":5}}", keep));
    assert(!tuxblox::applyBloxstrapRpcLine("[BloxstrapRPC] not json at all", keep));
    assert(!tuxblox::applyBloxstrapRpcLine("[BloxstrapRPC] {\"command\":\"SetRichPresence\"}", keep));
    assert(!tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"" +
        std::string(5 * 1024 * 1024, 'x') + "\"}}", keep));
    assert(keep == before);

    // Overlong text is cut to what Discord accepts, and control characters never reach it.
    tuxblox::RpcOverride clamped;
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"" +
        std::string(400, 'a') + "\"}}", clamped));
    assert(clamped.details.value.size() <= 128);

    // Discord counts characters, not bytes, so a status that is not Latin script keeps its length.
    const std::string jp = "\xe3\x81\x82";  // one 3-byte character
    std::string many;
    for (int i = 0; i < 128; i++) many += jp;
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"" + many + "\"}}", clamped));
    std::size_t characters = 0;
    for (const unsigned char c : clamped.details.value) {
        if ((c & 0xc0) != 0x80) characters++;
    }
    assert(characters == 128);
    // And the cut still lands on a character boundary, so no half character survives.
    assert(clamped.details.value.size() == 128 * 3);
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"state\":\"a\\nb\\tc\"}}", clamped));
    assert(clamped.state.value.find('\n') == std::string::npos);
    assert(clamped.state.value.find('\t') == std::string::npos);

    // The protocol's other command, which is what feeds the join link.
    tuxblox::RpcOverride launch;
    assert(tuxblox::applyBloxstrapRpcLine(
        "[FLog::Output] [BloxstrapRPC] {\"command\":\"SetLaunchData\",\"data\":{\"data\":\"room=7\"}}", launch));
    assert(launch.launchData == "room=7");

    // Roblox will not carry more than 200 bytes of it, so neither will we.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetLaunchData\",\"data\":{\"data\":\"" + std::string(400, 'd') + "\"}}", launch));
    assert(launch.launchData.size() == 200);

    // A blank one drops it, as the protocol's erase rule says.
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetLaunchData\",\"data\":{\"data\":\"\"}}", launch));
    assert(launch.launchData.empty());

    // And it never disturbs what the presence itself is showing.
    tuxblox::RpcOverride both;
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetRichPresence\",\"data\":{\"details\":\"Floor 3\"}}", both));
    assert(tuxblox::applyBloxstrapRpcLine(
        "[BloxstrapRPC] {\"command\":\"SetLaunchData\",\"data\":{\"data\":\"x\"}}", both));
    assert(both.details.value == "Floor 3" && both.launchData == "x");

    printf("bloxstrap_rpc: all tests passed\n");
    return 0;
}
