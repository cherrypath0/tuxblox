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
#include <unistd.h>

namespace tuxblox {

namespace {

struct Remembered {
    bool had = false;
    std::string value;
};

bool Recorded = false;
Remembered FontConfigFile;
Remembered SchemaDir;

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
}

void restoreBundledEnvironment() {
    if (!Recorded) return;
    put("FONTCONFIG_FILE", FontConfigFile);
    put("GSETTINGS_SCHEMA_DIR", SchemaDir);
    Recorded = false;
}

} // namespace tuxblox
