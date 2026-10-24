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
#include <cstdint>
#include <optional>
#include <string>

namespace tuxblox {

// Fetches a URL and returns its body. Named for the manifest it was written to fetch; it is the
// plain text fetcher now, used for the release pointer and the release file too.
// `httpStatus`, when given, receives the HTTP status before any exception is thrown -- 0 for a
// transport failure that never got one. It is what lets a caller tell "this is not published" from
// "something went wrong", which are different things to show a person.
std::string fetchManifestJson(const std::string& url, const std::atomic<bool>* cancel = nullptr,
                               long* httpStatus = nullptr);

// The release `channel` currently publishes, from client/releases/<channel>/latest.
//
// std::nullopt covers every way there is nothing to update to: a channel with nothing published
// yet, and a pointer that is not a release version. Those are the same answer to a caller, and
// treating a malformed pointer as a release would send it on to build a URL that cannot resolve.
std::optional<std::string> fetchLatestRelease(const std::string& baseUrl, const std::string& channel,
                                               const std::atomic<bool>* cancel = nullptr);

} // namespace tuxblox
