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

#include <cstdlib>
#include <cstring>
#include <unistd.h>

extern char **environ;

namespace tuxblox {

namespace {

struct Remembered {
    bool had = false;
    std::string value;
};

bool Recorded = false;
Remembered FontConfigFile;
Remembered SchemaDir;
std::string IconDir;

void remember(const char *pName, Remembered &into) {
    const char *pValue = getenv(pName);
    into.had = pValue != nullptr;
    into.value = pValue ? pValue : "";
}

void put(const char *pName, const Remembered &from) {
    if (from.had) {
        setenv(pName, from.value.c_str(), 1);
    } else {
        unsetenv(pName);
    }
}

bool names(const char *pEntry, const char *pName) {
    const size_t length = strlen(pName);
    return strncmp(pEntry, pName, length) == 0 && pEntry[length] == '=';
}

} // namespace

void useBundledEnvironment(const std::string &stackRoot) {
    if (!Recorded) {
        remember("FONTCONFIG_FILE", FontConfigFile);
        remember("GSETTINGS_SCHEMA_DIR", SchemaDir);
        Recorded = true;
    }

    // Fontconfig has no configuration of its own here, so without this the bundled fonts are never scanned on a machine that lacks /etc/fonts.
    const std::string config = stackRoot + "/fonts/fonts.conf";
    if (access(config.c_str(), R_OK) == 0) setenv("FONTCONFIG_FILE", config.c_str(), 1);

    // GLib otherwise looks under the prefix the stack was built for, finds no schemas and settings such as the colour scheme silently stop working.
    const std::string schemas = stackRoot + "/share/glib-2.0/schemas";
    if (access((schemas + "/gschemas.compiled").c_str(), R_OK) == 0) setenv("GSETTINGS_SCHEMA_DIR", schemas.c_str(), 1);

    // Not a variable: applyLook() hands this to the interface itself. An install from before TuxBlox shipped icons has none, and the empty answer is what makes the caller leave the host's alone.
    const std::string icons = stackRoot + "/share/icons";
    IconDir = access((icons + "/TuxBlox/index.theme").c_str(), R_OK) == 0 ? icons : std::string();
}

void restoreBundledEnvironment() {
    IconDir.clear();
    if (!Recorded) return;
    put("FONTCONFIG_FILE", FontConfigFile);
    put("GSETTINGS_SCHEMA_DIR", SchemaDir);
    Recorded = false;
}

std::string bundledIconDir() {
    return IconDir;
}

std::vector<std::string> unbundledEnvironment() {
    std::vector<std::string> entries;
    for (char **ppEntry = environ; *ppEntry != nullptr; ++ppEntry) {
        if (Recorded && (names(*ppEntry, "FONTCONFIG_FILE") || names(*ppEntry, "GSETTINGS_SCHEMA_DIR"))) continue;
        entries.push_back(*ppEntry);
    }
    if (!Recorded) return entries;
    if (FontConfigFile.had) entries.push_back("FONTCONFIG_FILE=" + FontConfigFile.value);
    if (SchemaDir.had) entries.push_back("GSETTINGS_SCHEMA_DIR=" + SchemaDir.value);
    return entries;
}

} // namespace tuxblox
