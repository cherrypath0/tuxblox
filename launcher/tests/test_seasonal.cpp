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

#include <cassert>
#include <ctime>

namespace {

std::time_t utc(int year, int month, int day, int hour, int minute, int second) {
    std::tm parts{};
    parts.tm_year = year - 1900;
    parts.tm_mon = month - 1;
    parts.tm_mday = day;
    parts.tm_hour = hour;
    parts.tm_min = minute;
    parts.tm_sec = second;
    return timegm(&parts);
}

} // namespace

int main() {
    using tuxblox::christmasThemeAvailable;

    // The window opens on 9 November 2026 and closes at the end of 6 January 2027, in UTC
    assert(!christmasThemeAvailable(utc(2026, 11, 8, 23, 59, 59)));
    assert(christmasThemeAvailable(utc(2026, 11, 9, 0, 0, 0)));
    assert(christmasThemeAvailable(utc(2026, 11, 30, 12, 0, 0)));
    assert(christmasThemeAvailable(utc(2026, 12, 25, 12, 0, 0)));
    assert(christmasThemeAvailable(utc(2026, 12, 31, 23, 59, 59)));
    assert(christmasThemeAvailable(utc(2027, 1, 1, 0, 0, 0)));
    assert(christmasThemeAvailable(utc(2027, 1, 6, 23, 59, 59)));
    assert(!christmasThemeAvailable(utc(2027, 1, 7, 0, 0, 0)));

    // A one-off for this season, so neither the year before nor the year after offers it
    assert(!christmasThemeAvailable(utc(2025, 12, 25, 12, 0, 0)));
    assert(!christmasThemeAvailable(utc(2027, 12, 25, 12, 0, 0)));
    assert(!christmasThemeAvailable(utc(2028, 1, 1, 12, 0, 0)));

    // A clock far enough out to be nonsense still answers rather than reading uninitialised time
    assert(!christmasThemeAvailable(0));

    return 0;
}
