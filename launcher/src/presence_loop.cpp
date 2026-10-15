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

#include "presence_loop.h"
#include "bloxstrap_rpc.h"
#include "discord_rpc.h"
#include "player_presence.h"
#include "roblox_place_info.h"
#include "roblox_universe.h"
#include "studio_presence.h"
#include <chrono>
#include <map>
#include <memory>
#include <thread>
#include <vector>

namespace tuxblox {

// What Roblox told us about a place, asked for once and kept for the session.
struct ResolvedPlace {
    std::string name;
    std::string iconUrl;
    // How the experience's author is credited. Asked for only on a Player session, because Studio shows the place a builder opened rather than an experience somebody is playing.
    std::string creatorLine;
};

// An asset a game named is described by Roblox, not served by it, so it has to be looked up before Discord can be given anything -- once per asset, failures remembered too, or a game rotating its art would mean a request every tick.
void resolveRpcImage(RpcImage& image, std::map<std::string, std::string>& resolved) {
    if (image.assetId.state != RpcField::State::Set || image.assetId.value.empty()) {
        return;
    }
    auto known = resolved.find(image.assetId.value);
    if (known == resolved.end()) {
        known = resolved
                    .emplace(image.assetId.value,
                             fetchThumbnailImageUrl(rpcAssetLookupUrl(image.assetId.value)))
                    .first;
    }
    image.resolvedUrl = known->second;
}

// The Discord applications this presence is published under, registered at discord.com/developers. Public, not secrets. Each program reports under its own, because Discord shows the application's own name above the activity and nothing sent over the socket can change it.
const char kStudioApplicationId[] = "1554221498925973584";
const char kPlayerApplicationId[] = "1557149187102220362";

bool presenceWatchWanted(const Settings& settings) {
    return settings.discordRpc || settings.serverRegionNotify;
}

std::string presenceNotificationIcon(LaunchTarget target) {
    return target == LaunchTarget::Player ? "tuxblox-roblox-player" : "tuxblox-roblox-studio";
}

void runPresenceLoop(const std::string& installDir, LaunchTarget target, const Settings& settings,
                     std::time_t launchStart, const std::function<bool()>& sessionIsRunning,
                     ServerRegionWatcher regionWatcher, const std::string& logPath) {
    // One switch covers both programs, and a game may change what is shown whenever presence is on.
    const bool presenceWanted = settings.discordRpc;
    const bool player = target == LaunchTarget::Player;
    std::unique_ptr<DiscordRpc> discord;
    std::unique_ptr<SessionLogTail> logTail;
    std::unique_ptr<PresenceReader> presenceReader;
    if (player) {
        presenceReader = std::make_unique<PlayerPresenceReader>();
    } else {
        presenceReader = std::make_unique<StudioPresenceReader>();
    }
    PresenceActivity lastSent;
    std::map<std::string, ResolvedPlace> resolvedPlaces;
    bool everSent = false;
    std::vector<std::time_t> recentSends;
    std::map<std::string, std::string> resolvedAssets;
    std::string lastServerAddress;
    if (presenceWanted) {
        discord = std::make_unique<DiscordRpc>(player ? kPlayerApplicationId : kStudioApplicationId);
    }

    while (sessionIsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        // The log is read for either feature, so neither one being on is what stops the work.
        if (!presenceWanted && !settings.serverRegionNotify) {
            continue;
        }

        const std::time_t now = std::time(nullptr);
        if (!logTail) {
            const std::string logFile =
                logPath.empty() ? findSessionLog(installDir, launchStart, player) : logPath;
            if (!logFile.empty()) {
                logTail = std::make_unique<SessionLogTail>(logFile);
            }
        }
        if (logTail) {
            logTail->pump(*presenceReader);
        }

        // Each server is named once, and a reader whose target never reports one answers empty.
        const std::string serverAddress = presenceReader->serverAddress();
        if (!serverAddress.empty() && serverAddress != lastServerAddress) {
            lastServerAddress = serverAddress;
            regionWatcher.consumeServerAddress(serverAddress);
        }

        if (!presenceWanted) {
            continue;
        }

        discord->poll(now);
        PresenceActivity current = presenceReader->activity();
        // Dropped here rather than further down, so a game that asks for a status nobody wanted cannot make this fetch its art, or spend one of the few updates Discord allows on a presence that would read the same.
        if (!settings.discordGameStatus) {
            current.rpc = RpcOverride{};
        }

        // Studio names a published place only by number, so the name is asked for once per place and remembered, including when Roblox has none to give.
        // Roblox's own report is preferred over the join line, which a game could have written.
        const std::string describeId =
            current.reportedPlaceId.empty() ? current.placeId : current.reportedPlaceId;
        if (!describeId.empty()) {
            auto known = resolvedPlaces.find(describeId);
            if (known == resolvedPlaces.end()) {
                ResolvedPlace fetched;
                // A player knows the experience by its own name and its author, which is a different question from what the place inside it is called.
                UniverseInfo universe;
                const bool described = !current.universeId.empty()
                                           ? fetchUniverseInfoById(current.universeId, universe)
                                           : fetchUniverseInfo(describeId, universe);
                if (described) {
                    fetched.name = universe.name;
                    fetched.creatorLine = creatorLine(universe);
                } else if (current.placeName.empty()) {
                    fetched.name = fetchPlaceName(describeId);
                }
                // The experience's own icon, which belongs to its root place -- teleporting into a lobby inside it must not change the picture on somebody's profile.
                const std::string iconPlaceId =
                    described && !universe.rootPlaceId.empty() ? universe.rootPlaceId : describeId;
                fetched.iconUrl = fetchPlaceIconUrl(iconPlaceId);
                known = resolvedPlaces.emplace(describeId, fetched).first;
            }
            if (!known->second.name.empty()) {
                current.placeName = known->second.name;
            }
            current.placeIconUrl = known->second.iconUrl;
            current.creatorLine = known->second.creatorLine;
        }
        // Only a delivered activity counts: recording one Discord threw away would stop this ever trying again.
        resolveRpcImage(current.rpc.largeImage, resolvedAssets);
        resolveRpcImage(current.rpc.smallImage, resolvedAssets);

        if ((!everSent || current != lastSent) && rpcAllowsSend(recentSends, now) &&
            discord->send(activityJson(current, launchStart, settings.discordJoinButton), now)) {
            lastSent = current;
            everSent = true;
            recentSends.push_back(now);
        }
    }

    if (discord) {
        discord->clear(std::time(nullptr));
    }
}

} // namespace tuxblox
