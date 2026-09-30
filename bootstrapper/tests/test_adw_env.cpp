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

#include "adw_env.h"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace fs = std::filesystem;

namespace {

fs::path makeStack(bool withFonts, bool withSchemas) {
    const fs::path root = fs::temp_directory_path() / ("adw_env_test_" + std::to_string(getpid()) + (withFonts ? "f" : "n") + (withSchemas ? "s" : "n"));
    fs::remove_all(root);
    fs::create_directories(root / "fonts");
    fs::create_directories(root / "share/glib-2.0/schemas");
    if (withFonts) std::ofstream(root / "fonts/fonts.conf") << "<fontconfig/>";
    if (withSchemas) std::ofstream(root / "share/glib-2.0/schemas/gschemas.compiled") << "x";
    return root;
}

bool isSet(const char *pName) {
    return getenv(pName) != nullptr;
}

std::string valueOf(const char *pName) {
    const char *pValue = getenv(pName);
    return pValue ? pValue : "<unset>";
}

void clearBoth() {
    unsetenv("FONTCONFIG_FILE");
    unsetenv("GSETTINGS_SCHEMA_DIR");
}

// A full stack points both variables into it and restoring brings back exactly what was there
void pointsIntoTheStackAndRestoresPriorValues() {
    const fs::path root = makeStack(true, true);
    setenv("FONTCONFIG_FILE", "/host/fonts.conf", 1);
    setenv("GSETTINGS_SCHEMA_DIR", "/host/schemas", 1);

    tuxblox::useBundledEnvironment(root.string());
    assert(valueOf("FONTCONFIG_FILE") == (root / "fonts/fonts.conf").string());
    assert(valueOf("GSETTINGS_SCHEMA_DIR") == (root / "share/glib-2.0/schemas").string());

    tuxblox::restoreBundledEnvironment();
    assert(valueOf("FONTCONFIG_FILE") == "/host/fonts.conf");
    assert(valueOf("GSETTINGS_SCHEMA_DIR") == "/host/schemas");
    fs::remove_all(root);
}

// Unset must come back unset, because an empty FONTCONFIG_FILE is not the same as none
void unsetComesBackUnsetNotEmpty() {
    const fs::path root = makeStack(true, true);
    clearBoth();

    tuxblox::useBundledEnvironment(root.string());
    assert(isSet("FONTCONFIG_FILE") && isSet("GSETTINGS_SCHEMA_DIR"));

    tuxblox::restoreBundledEnvironment();
    assert(!isSet("FONTCONFIG_FILE"));
    assert(!isSet("GSETTINGS_SCHEMA_DIR"));
    fs::remove_all(root);
}

// Only what the stack actually contains is pointed at
void leavesAVariableAloneWhenItsFileIsMissing() {
    const fs::path fontsOnly = makeStack(true, false);
    setenv("FONTCONFIG_FILE", "/host/fonts.conf", 1);
    setenv("GSETTINGS_SCHEMA_DIR", "/host/schemas", 1);

    tuxblox::useBundledEnvironment(fontsOnly.string());
    assert(valueOf("FONTCONFIG_FILE") == (fontsOnly / "fonts/fonts.conf").string());
    assert(valueOf("GSETTINGS_SCHEMA_DIR") == "/host/schemas");
    tuxblox::restoreBundledEnvironment();
    fs::remove_all(fontsOnly);

    const fs::path schemasOnly = makeStack(false, true);
    tuxblox::useBundledEnvironment(schemasOnly.string());
    assert(valueOf("FONTCONFIG_FILE") == "/host/fonts.conf");
    assert(valueOf("GSETTINGS_SCHEMA_DIR") == (schemasOnly / "share/glib-2.0/schemas").string());
    tuxblox::restoreBundledEnvironment();
    fs::remove_all(schemasOnly);
}

// A second call must not make the first call's values the ones that get restored
void secondCallKeepsTheOriginalMemory() {
    const fs::path root = makeStack(true, true);
    setenv("FONTCONFIG_FILE", "/host/fonts.conf", 1);
    unsetenv("GSETTINGS_SCHEMA_DIR");

    tuxblox::useBundledEnvironment(root.string());
    tuxblox::useBundledEnvironment(root.string());
    tuxblox::restoreBundledEnvironment();

    assert(valueOf("FONTCONFIG_FILE") == "/host/fonts.conf");
    assert(!isSet("GSETTINGS_SCHEMA_DIR"));
    fs::remove_all(root);
}

void restoreWithoutUseChangesNothing() {
    setenv("FONTCONFIG_FILE", "/untouched", 1);
    unsetenv("GSETTINGS_SCHEMA_DIR");

    tuxblox::restoreBundledEnvironment();

    assert(valueOf("FONTCONFIG_FILE") == "/untouched");
    assert(!isSet("GSETTINGS_SCHEMA_DIR"));
}

// Test helper: the value a NAME=value list gives a variable, or "<unset>"; a name listed twice is itself a failure
std::string entryValue(const std::vector<std::string> &entries, const std::string &name) {
    std::string found = "<unset>";
    int count = 0;
    for (const std::string &entry : entries) {
        if (entry.compare(0, name.size() + 1, name + "=") != 0) continue;
        found = entry.substr(name.size() + 1);
        ++count;
    }
    assert(count <= 1);
    return found;
}

// A started program gets back what the bundled environment replaced, while this process keeps the bundled values
void unbundledEnvironmentPutsTheOriginalsBack() {
    const fs::path root = makeStack(true, true);
    setenv("FONTCONFIG_FILE", "/host/fonts.conf", 1);
    unsetenv("GSETTINGS_SCHEMA_DIR");
    setenv("TUXBLOX_TEST_PASSTHROUGH", "kept", 1);

    tuxblox::useBundledEnvironment(root.string());
    const std::vector<std::string> entries = tuxblox::unbundledEnvironment();

    assert(entryValue(entries, "FONTCONFIG_FILE") == "/host/fonts.conf");
    assert(entryValue(entries, "GSETTINGS_SCHEMA_DIR") == "<unset>");
    assert(entryValue(entries, "TUXBLOX_TEST_PASSTHROUGH") == "kept");
    assert(valueOf("FONTCONFIG_FILE") == (root / "fonts/fonts.conf").string());

    tuxblox::restoreBundledEnvironment();
    unsetenv("TUXBLOX_TEST_PASSTHROUGH");
    fs::remove_all(root);
}

// Only the exact names are the stack's, so a variable that merely starts with one passes through
void unbundledEnvironmentMatchesWholeNames() {
    const fs::path root = makeStack(true, true);
    clearBoth();
    setenv("FONTCONFIG_FILE_EXTRA", "other", 1);

    tuxblox::useBundledEnvironment(root.string());
    const std::vector<std::string> entries = tuxblox::unbundledEnvironment();

    assert(entryValue(entries, "FONTCONFIG_FILE_EXTRA") == "other");
    assert(entryValue(entries, "FONTCONFIG_FILE") == "<unset>");
    tuxblox::restoreBundledEnvironment();
    unsetenv("FONTCONFIG_FILE_EXTRA");
    fs::remove_all(root);
}

// With nothing bundled there is nothing to put back
void unbundledEnvironmentWithoutUseIsEnviron() {
    setenv("FONTCONFIG_FILE", "/host/fonts.conf", 1);
    unsetenv("GSETTINGS_SCHEMA_DIR");
    std::vector<std::string> expected;
    for (char **ppEntry = environ; *ppEntry != nullptr; ++ppEntry) expected.push_back(*ppEntry);
    assert(tuxblox::unbundledEnvironment() == expected);
}

} // namespace

int main() {
    pointsIntoTheStackAndRestoresPriorValues();
    unsetComesBackUnsetNotEmpty();
    leavesAVariableAloneWhenItsFileIsMissing();
    secondCallKeepsTheOriginalMemory();
    restoreWithoutUseChangesNothing();
    unbundledEnvironmentPutsTheOriginalsBack();
    unbundledEnvironmentMatchesWholeNames();
    unbundledEnvironmentWithoutUseIsEnviron();
    return 0;
}
