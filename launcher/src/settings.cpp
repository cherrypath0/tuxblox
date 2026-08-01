#include "settings.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string settingsFilePath(const std::string& installDir) {
    return installDir + "/launcher_settings.json";
}

} // namespace

Settings loadSettings(const std::string& installDir) {
    Settings settings;
    try {
        std::ifstream file(settingsFilePath(installDir));
        if (!file) return settings;

        nlohmann::json j;
        file >> j;

        settings.protonEnvVars = j.at("proton_env_vars").get<std::string>();
        settings.globalEnvVars = j.at("global_env_vars").get<std::string>();
        settings.sendCrashReports = j.at("send_crash_reports").get<bool>();
        return settings;
    } catch (...) {
        // Missing file, unreadable file, parse error, or a missing/wrong-typed
        // field -- fall back to defaults wholesale rather than partially
        // applying whatever did parse. A corrupt settings file must never
        // crash the launcher.
        return Settings{};
    }
}

void saveSettings(const std::string& installDir, const Settings& settings) {
    try {
        std::error_code ec;
        fs::create_directories(installDir, ec);

        nlohmann::json j;
        j["proton_env_vars"] = settings.protonEnvVars;
        j["global_env_vars"] = settings.globalEnvVars;
        j["send_crash_reports"] = settings.sendCrashReports;

        std::ofstream file(settingsFilePath(installDir), std::ios::binary);
        if (!file) return;
        file << j.dump(2);
    } catch (...) {
        // Best-effort -- a failed settings save must not crash the launcher.
    }
}

std::vector<std::string> parseEnvPairs(const std::string& text) {
    std::vector<std::string> pairs;
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        if (token.find('=') != std::string::npos) {
            pairs.push_back(token);
        }
    }
    return pairs;
}

} // namespace tuxblox
