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

#include "FlagManager.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace tuxblox;

    // The prefix states the type, as Roblox's own flags do.
    {
        assert(flagTypeForName("tbxFlagEnableChristmasTheme").value() == FlagType::Bool);
        assert(flagTypeForName("tbxStringPrivacyPolicyVersion").value() == FlagType::String);
        assert(flagTypeForName("tbxIntRobloxInstallerParallelWorkers").value() == FlagType::Int);
        assert(!flagTypeForName("somethingElse").has_value());
        assert(!flagTypeForName("tbxFlag").has_value());   // a prefix and nothing else is not a name
        assert(!flagTypeForName("tbx").has_value());
        assert(!flagTypeForName("").has_value());
    }

    // A value that contradicts its own name is dropped, so the next source down answers.
    // Keeping it would mean every install guessing the same wrong thing.
    {
        assert(parseFlagJson(R"({"tbxFlagA":"yes","tbxIntB":4})", false).size() == 1);
        assert(parseFlagJson(R"({"tbxIntB":"4"})", false).empty());
        assert(parseFlagJson(R"({"tbxStringC":9})", false).empty());
        assert(parseFlagJson(R"({"tbxFlagA":1})", false).empty());       // 1 is not true
        assert(parseFlagJson(R"({"tbxIntB":4.5})", false).empty());      // not an integer
        // Past int64: nlohmann parses this as UNSIGNED, is_number_integer() is true, and
        // get<int64_t>() wraps it to a large negative. A flag set large and positive
        // would arrive at every install as its negative twin, so it is refused instead.
        assert(parseFlagJson(R"({"tbxIntB":9223372036854775808})", false).empty());
        assert(parseFlagJson(R"({"tbxIntB":18446744073709551615})", false).empty());
        assert(parseFlagJson(R"({"tbxIntB":1e300})", false).empty());
        // The edges themselves still work.
        assert(parseFlagJson(R"({"tbxIntB":9223372036854775807})", false)
                   .at("tbxIntB").integer == 9223372036854775807LL);
        assert(parseFlagJson(R"({"unknownPrefix":true})", false).empty());
        assert(parseFlagJson(R"({"tbxFlagA":{"nested":true}})", false).empty());
        assert(parseFlagJson(R"({"tbxFlagA":[true]})", false).empty());
        assert(parseFlagJson(R"({"tbxFlagA":null})", false).empty());
    }

    // Not a flat object at all. Every one of these leaves the lower sources alone.
    {
        assert(parseFlagJson("[1,2,3]", false).empty());
        assert(parseFlagJson("\"a string\"", false).empty());
        assert(parseFlagJson("42", false).empty());
        assert(parseFlagJson("null", false).empty());
        assert(parseFlagJson("not json at all", false).empty());
        assert(parseFlagJson("", false).empty());
        assert(parseFlagJson("{", false).empty());
    }

    // The client enforces the local-only list; the server is not trusted to respect it.
    // tbxFlagDisableUpdater from the server would be a kill switch that cannot be
    // corrected by updating, because updating is what it stops.
    {
        assert(isLocalOnlyFlag("tbxFlagDisableUpdater"));
        assert(!isLocalOnlyFlag("tbxFlagEnableChristmasTheme"));
        assert(parseFlagJson(R"({"tbxFlagDisableUpdater":true})", true).empty());
        assert(parseFlagJson(R"({"tbxFlagDisableUpdater":true})", false).size() == 1);
        // The rest of a server answer survives its presence.
        assert(parseFlagJson(R"({"tbxFlagDisableUpdater":true,"tbxFlagA":true})", true).size() == 1);
    }

    // Values round-trip with their type.
    {
        const FlagTable t = parseFlagJson(
            R"({"tbxFlagA":true,"tbxStringB":"2026-10-16","tbxIntC":6})", false);
        assert(t.size() == 3);
        assert(t.at("tbxFlagA").boolean == true);
        assert(t.at("tbxStringB").text == "2026-10-16");
        assert(t.at("tbxIntC").integer == 6);
        // A negative and a large integer survive as themselves.
        const FlagTable big = parseFlagJson(R"({"tbxIntA":-1,"tbxIntB":9007199254740993})", false);
        assert(big.at("tbxIntA").integer == -1);
        assert(big.at("tbxIntB").integer == 9007199254740993LL);
    }

    // Merge is per key: a key the higher source does not set is left alone, and a key
    // only the higher source has is added -- which is how a flag is introduced ahead of
    // the release that reads it.
    {
        FlagTable base = parseFlagJson(R"({"tbxFlagA":false,"tbxIntB":1})", false);
        const FlagTable higher = parseFlagJson(R"({"tbxFlagA":true,"tbxStringC":"x"})", false);
        mergeFlagsOver(base, higher);
        assert(base.size() == 3);
        assert(base.at("tbxFlagA").boolean == true);   // overridden
        assert(base.at("tbxIntB").integer == 1);       // untouched
        assert(base.at("tbxStringC").text == "x");     // introduced
    }

    // Merging an empty table changes nothing -- which is what a failed fetch looks like.
    {
        FlagTable base = parseFlagJson(R"({"tbxFlagA":true})", false);
        mergeFlagsOver(base, FlagTable{});
        assert(base.size() == 1 && base.at("tbxFlagA").boolean == true);
    }

    printf("flag_table: all tests passed\n");
    return 0;
}
