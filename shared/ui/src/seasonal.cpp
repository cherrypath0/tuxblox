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

#include "seasonal.h"

namespace tuxblox {

bool christmasThemeAvailable(std::time_t now) {
    std::tm utc{};
    if (gmtime_r(&now, &utc) == nullptr) return false;

    const int year = utc.tm_year + 1900;
    const int month = utc.tm_mon + 1;
    const int day = utc.tm_mday;

    // A one-off for the 2026 season rather than something that returns every year
    if (year == 2026) return month == 12 || (month == 11 && day >= 9);
    if (year == 2027) return month == 1 && day <= 6;
    return false;
}

bool christmasThemeAvailable() {
    return christmasThemeAvailable(std::time(nullptr));
}

} // namespace tuxblox
