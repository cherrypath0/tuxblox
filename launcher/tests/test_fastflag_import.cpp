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

#include "fastflag_import.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using tuxblox::FastFlag;
using tuxblox::parseFastFlagJson;

bool has(const std::vector<FastFlag> &flags, const std::string &name, const std::string &value) {
    for (const FastFlag &flag : flags) {
        if (flag.name == name && flag.value == value) return true;
    }
    return false;
}

} // namespace

int main() {
    // The case from the feature: string values, kept in the order they were written
    {
        const auto result = parseFastFlagJson("{\"SomeFlag\": \"true\", \"SomeFlag2\": \"false\"}");
        assert(result.ok);
        assert(result.flags.size() == 2);
        assert(result.flags[0].name == "SomeFlag" && result.flags[0].value == "true");
        assert(result.flags[1].name == "SomeFlag2" && result.flags[1].value == "false");
    }

    // Roblox's own flag files hold real booleans and numbers, which become the text a flag file stores
    {
        const auto result = parseFastFlagJson("{\"A\": true, \"B\": false, \"C\": 30, \"D\": 0.5, \"E\": -2}");
        assert(result.ok);
        assert(has(result.flags, "A", "true") && has(result.flags, "B", "false"));
        assert(has(result.flags, "C", "30") && has(result.flags, "D", "0.5") && has(result.flags, "E", "-2"));
    }

    // Whitespace, newlines and a pretty-printed file are fine
    assert(parseFastFlagJson("  {\n  \"A\": \"1\",\n  \"B\": \"2\"\n}\n").ok);

    // Anything that is not valid JSON is refused with a reason the user can read
    for (const char *pBroken : {"{\"A\": \"1\"", "{\"A\": \"1\",}", "{A: 1}", "not json", "{\"A\" \"1\"}", "{'A': '1'}"}) {
        const auto result = parseFastFlagJson(pBroken);
        assert(!result.ok);
        assert(!result.error.empty());
        assert(result.error.find("json.exception") == std::string::npos);
        assert(result.flags.empty());
    }

    // Valid JSON that is not a list of flags is refused too
    assert(!parseFastFlagJson("[1, 2]").ok);
    assert(!parseFastFlagJson("\"text\"").ok);
    assert(!parseFastFlagJson("42").ok);
    assert(!parseFastFlagJson("{\"A\": null}").ok);
    assert(!parseFastFlagJson("{\"A\": {\"B\": 1}}").ok);
    assert(!parseFastFlagJson("{\"A\": [1]}").ok);
    assert(!parseFastFlagJson("{\"\": \"1\"}").ok);

    // Nothing to import is an answer the user should be told, not a silent success
    {
        const auto empty = parseFastFlagJson("{}");
        assert(!empty.ok);
        assert(!empty.error.empty());
        assert(!parseFastFlagJson("").ok);
        assert(!parseFastFlagJson("   \n ").ok);
    }

    // Importing over what is already there: same name takes the new value in place, new names go on the end
    {
        const std::vector<FastFlag> existing = {{"Keep", "1"}, {"Change", "old"}, {"Last", "z"}};
        const std::vector<FastFlag> imported = {{"Change", "new"}, {"Fresh", "x"}};
        const auto merged = tuxblox::mergeFastFlags(existing, imported);
        assert(merged.size() == 4);
        assert(merged[0].name == "Keep" && merged[0].value == "1");
        assert(merged[1].name == "Change" && merged[1].value == "new");
        assert(merged[2].name == "Last" && merged[2].value == "z");
        assert(merged[3].name == "Fresh" && merged[3].value == "x");
    }

    printf("fastflag_import: all tests passed\n");
    return 0;
}
