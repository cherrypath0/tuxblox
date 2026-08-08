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
#include <functional>

namespace tuxblox {

// Testable core. `getenvFn` mimics std::getenv (returns nullptr if unset);
// `containerEnvExists` mimics fs::exists("/run/.containerenv"). True only
// when both distrobox's own marker (the CONTAINER_ID env var it exports
// inside the container) and the generic OCI container marker are present
// -- requiring both avoids false positives from unrelated Podman/Toolbox
// containers that only set one of the two.
bool isInsideDistrobox(const std::function<const char*(const char*)>& getenvFn,
                        bool containerEnvExists);

// Convenience overload using the real environment.
bool isInsideDistrobox();

} // namespace tuxblox
