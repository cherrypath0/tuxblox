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

#include "i18n.h"
#include "i18n_data.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// Only initializeFromSettingsFile needs it; the catalogs themselves are
// generated as plain C++ tables by tools/embed-translations.py.
#include "json.hpp"

namespace tuxblox::i18n {

namespace {

Language CurrentLanguage = Language::English;
bool Initialized = false;

using StringMap = std::unordered_map<std::string, std::string>;
using ContextMap = std::unordered_map<std::string, StringMap>;

StringMap SelectedStrings;
StringMap EnglishStrings;
ContextMap SelectedContexts;
ContextMap EnglishContexts;

std::string lowered(std::string text) {
    for (char &c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string normalizedTag(std::string_view tag) {
    std::string text(tag);
    const size_t suffix = text.find_first_of(".@");
    if (suffix != std::string::npos) text.resize(suffix);
    text = lowered(text);
    for (char &c : text) {
        if (c == '-') c = '_';
    }
    return text;
}

bool startsWith(const std::string &text, const char *prefix) {
    return text.rfind(prefix, 0) == 0;
}

const I18nCatalog *catalogFor(Language language) {
    const char *code = "en";
    switch (language) {
        case Language::English: code = "en"; break;
        case Language::PortugueseBrazil: code = "pt_BR"; break;
        case Language::Spanish: code = "es"; break;
    }
    for (size_t i = 0; i < sizeof(kI18nCatalogs) / sizeof(kI18nCatalogs[0]); ++i) {
        if (std::string(kI18nCatalogs[i].code) == code) return &kI18nCatalogs[i];
    }
    return nullptr;
}

void loadInto(const I18nCatalog *catalog, StringMap &strings, ContextMap &contexts) {
    if (catalog == nullptr) return;
    for (size_t i = 0; i < catalog->count; ++i) {
        const I18nEntry &entry = catalog->entries[i];
        if (entry.source == nullptr || entry.translation == nullptr) continue;
        if (entry.context != nullptr && entry.context[0] != '\0') {
            contexts[entry.context][entry.source] = entry.translation;
        } else {
            strings[entry.source] = entry.translation;
        }
    }
}

std::string formatPlaceholders(const std::string &text, std::initializer_list<std::string> args) {
    const std::vector<std::string> values(args);
    std::string out;
    out.reserve(text.size());
    size_t next = 0;
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '{') {
            if (i + 1 < text.size() && text[i + 1] == '}') {
                if (next < values.size()) out += values[next++];
                i += 2;
                continue;
            }
            size_t at = i + 1;
            while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) ++at;
            if (at > i + 1 && at < text.size() && text[at] == '}') {
                const size_t index = static_cast<size_t>(std::strtoul(text.substr(i + 1, at - i - 1).c_str(), nullptr, 10));
                if (index < values.size()) out += values[index];
                else out += text.substr(i, at - i + 1);
                i = at + 1;
                continue;
            }
        }
        out += text[i++];
    }
    return out;
}

std::optional<Language> firstLanguageIn(const char *value) {
    if (value == nullptr || *value == '\0') return std::nullopt;
    const std::string list(value);
    size_t at = 0;
    while (at <= list.size()) {
        const size_t end = list.find(':', at);
        const std::string part = list.substr(at, end == std::string::npos ? std::string::npos : end - at);
        if (const auto language = parseLanguage(part)) return language;
        if (end == std::string::npos) break;
        at = end + 1;
    }
    return std::nullopt;
}

} // namespace

std::optional<Language> parseLanguage(std::string_view value) {
    const std::string text = normalizedTag(value);
    if (text.empty()) return std::nullopt;
    if (text == "en" || startsWith(text, "en_")) return Language::English;
    if (text == "pt" || startsWith(text, "pt_")) return Language::PortugueseBrazil;
    if (text == "es" || startsWith(text, "es_")) return Language::Spanish;
    return std::nullopt;
}

Language systemLanguage() {
    for (const char *name : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (const auto language = firstLanguageIn(std::getenv(name))) return *language;
    }
    return Language::English;
}

std::string tag(Language language) {
    switch (language) {
        case Language::PortugueseBrazil: return "pt_BR";
        case Language::Spanish: return "es";
        case Language::English: break;
    }
    return "en";
}

const char *nativeName(Language language) {
    switch (language) {
        case Language::PortugueseBrazil: return "Português (Brasil)";
        case Language::Spanish: return "Español";
        case Language::English: break;
    }
    return "English";
}

void setLanguage(Language language) {
    CurrentLanguage = language;

    EnglishStrings.clear();
    EnglishContexts.clear();
    loadInto(catalogFor(Language::English), EnglishStrings, EnglishContexts);

    SelectedStrings.clear();
    SelectedContexts.clear();
    if (language == Language::English) {
        SelectedStrings = EnglishStrings;
        SelectedContexts = EnglishContexts;
    } else {
        loadInto(catalogFor(language), SelectedStrings, SelectedContexts);
    }

    Initialized = true;
    setenv("TUXBLOX_LANGUAGE", tag(language).c_str(), 1);
}

Language current() {
    return CurrentLanguage;
}

void initializeFromSetting(const std::string &setting) {
    if (const char *override = std::getenv("TUXBLOX_LANGUAGE"); override != nullptr && override[0] != '\0') {
        if (const auto language = parseLanguage(override)) {
            setLanguage(*language);
            return;
        }
    }
    if (setting != "system") {
        if (const auto language = parseLanguage(setting)) {
            setLanguage(*language);
            return;
        }
    }
    setLanguage(systemLanguage());
}

void initializeFromSettingsFile(const std::string &settingsJsonPath) {
    std::string setting = "system";
    try {
        std::ifstream file(settingsJsonPath, std::ios::binary);
        if (file) {
            nlohmann::json settings;
            file >> settings;
            setting = normalizeLanguageSetting(settings.value("language", std::string("system")));
        }
    } catch (...) {
        // A missing, unreadable or malformed settings file means the desktop's
        // language, exactly as a fresh install gets.
    }
    initializeFromSetting(setting);
}

void ensureInitialized() {
    if (!Initialized) initializeFromSetting("system");
}

const char *tr(const char *source) {
    if (source == nullptr || source[0] == '\0' || !Initialized) return source;
    const auto selected = SelectedStrings.find(source);
    if (selected != SelectedStrings.end()) return selected->second.c_str();
    const auto english = EnglishStrings.find(source);
    if (english != EnglishStrings.end()) return english->second.c_str();
    return source;
}

const char *trc(const char *context, const char *source) {
    if (context == nullptr || context[0] == '\0') return tr(source);
    if (Initialized) {
        const auto selected = SelectedContexts.find(context);
        if (selected != SelectedContexts.end()) {
            const auto found = selected->second.find(source);
            if (found != selected->second.end()) return found->second.c_str();
        }
        const auto english = EnglishContexts.find(context);
        if (english != EnglishContexts.end()) {
            const auto found = english->second.find(source);
            if (found != english->second.end()) return found->second.c_str();
        }
    }
    return tr(source);
}

std::string trf(const char *source, std::initializer_list<std::string> args) {
    return formatPlaceholders(tr(source), args);
}

std::string trn(const char *singular, const char *plural, long long count) {
    const char *chosen = count == 1 ? singular : plural;
    return formatPlaceholders(tr(chosen), {std::to_string(count)});
}

std::string normalizeLanguageSetting(const std::string &value) {
    if (lowered(value) == "system" || value.empty()) return "system";
    if (const auto language = parseLanguage(value)) return tag(*language);
    return "system";
}

} // namespace tuxblox::i18n
