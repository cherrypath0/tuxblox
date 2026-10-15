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
#include <ctime>
#include <functional>
#include <string>

#include "lnk_resolver.h"
#include "server_region.h"
#include "settings.h"

namespace tuxblox {

// Both features are driven by reading the session log, so either one being on is what decides whether a session has to be watched at all.
bool presenceWatchWanted(const Settings& settings);

// The icon the notification wears, which is the one the desktop shortcuts carry so it looks like it came from Roblox rather than from the network.
std::string presenceNotificationIcon(LaunchTarget target);

// Publishes the Discord presence and names the server's region for one session, returning once sessionIsRunning() reports it has ended.
//
// `sessionIsRunning` is the caller's own answer because the two launch routes know it differently: the watched launch tracks the process it started, while a launch handed its process to the game has to look for the session in the virtual drive. `logPath` overrides where the session log is looked for, which is what lets this be tested without a session.
void runPresenceLoop(const std::string& installDir, LaunchTarget target, const Settings& settings,
                     std::time_t launchStart, const std::function<bool()>& sessionIsRunning,
                     ServerRegionWatcher regionWatcher, const std::string& logPath = "");

} // namespace tuxblox
