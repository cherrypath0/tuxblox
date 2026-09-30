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
#include <string>

namespace tuxblox {

struct UiStackResult {
    bool ok = false;
    std::string uiBinaryPath;
    std::string errorMessage;
    std::string errorDetail;
};

// Makes the interface stack appended to `selfExe` available on disk and returns the path of the interface binary inside it. A complete extraction of the same payload is reused rather than redone. On failure errorMessage names the cause and what to do about it, and errorDetail holds the underlying system error for the log.
UiStackResult ensureUiStack(const std::string& selfExe, const std::string& version);

} // namespace tuxblox
