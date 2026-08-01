#pragma once
#include <string>
#include <vector>

namespace tuxblox {

struct Settings {
    // Space-separated "VAR=VALUE" pairs, in the same format launch.sh and
    // user_settings.sample.py already use elsewhere in this repo.
    std::string protonEnvVars;  // applied only to the "proton run" child
    std::string globalEnvVars;  // applied to the launcher process itself (setenv), inherited by everything it spawns
    bool sendCrashReports = true;
};

// installDir + "/launcher_settings.json". Never throws: a missing file,
// unreadable file, parse error, or malformed/missing fields all fall back
// to a default-constructed Settings{} -- a corrupt settings file must never
// crash the launcher.
Settings loadSettings(const std::string& installDir);
void saveSettings(const std::string& installDir, const Settings& settings);

// Splits a whitespace-separated string of "VAR=VALUE" tokens into
// "VAR=VALUE" strings, tolerant of repeated/extra whitespace. Tokens with
// no '=' are skipped (matches TrackedProcess::start's existing per-pair
// parsing convention in process_launcher.cpp).
std::vector<std::string> parseEnvPairs(const std::string& text);

} // namespace tuxblox
