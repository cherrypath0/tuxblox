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
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// Only initializeFromSettingsFile needs it; the catalogues themselves are
// generated as plain C++ tables by tools/embed-translations.py.
#include "json.hpp"

namespace tuxblox::i18n {

namespace {

std::string CurrentTag = "en";
bool Initialized = false;

using StringMap = std::unordered_map<std::string, std::string>;
using ContextMap = std::unordered_map<std::string, StringMap>;
using PluralMap = std::unordered_map<std::string, std::vector<std::string>>;

StringMap SelectedStrings;
ContextMap SelectedContexts;
PluralMap SelectedPlurals;
const I18nCatalog *SelectedCatalog = nullptr;

std::string lowered(std::string text) {
    for (char &c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

// "pt-BR.UTF-8@euro" and "pt_br" both come out as "pt_br", which is what the
// tags are compared in.
std::string normalizedTag(std::string_view tag) {
    std::string text(tag);
    const size_t suffix = text.find_first_of(".@");
    if (suffix != std::string::npos) text.resize(suffix);
    for (char &c : text) {
        if (c == '-') c = '_';
    }
    return lowered(text);
}

std::string baseOf(const std::string &normalized) {
    const size_t underscore = normalized.find('_');
    return underscore == std::string::npos ? normalized : normalized.substr(0, underscore);
}

const I18nCatalog *catalogFor(const std::string &tag) {
    for (size_t i = 0; i < kI18nCatalogCount; ++i) {
        if (normalizedTag(kI18nCatalogs[i].code) == normalizedTag(tag)) return &kI18nCatalogs[i];
    }
    return nullptr;
}

void loadInto(const I18nCatalog *catalog) {
    SelectedStrings.clear();
    SelectedContexts.clear();
    SelectedPlurals.clear();
    SelectedCatalog = catalog;
    if (catalog == nullptr) return;
    for (size_t i = 0; i < catalog->count; ++i) {
        const I18nEntry &entry = catalog->entries[i];
        if (entry.source == nullptr || entry.translation == nullptr) continue;
        if (entry.translation[0] == '\0') continue; // untranslated: the English source stands
        if (entry.context != nullptr && entry.context[0] != '\0') {
            SelectedContexts[entry.context][entry.source] = entry.translation;
        } else {
            SelectedStrings[entry.source] = entry.translation;
        }
    }
    for (size_t i = 0; i < catalog->pluralCount; ++i) {
        const I18nPlural &plural = catalog->plurals[i];
        if (plural.singular == nullptr) continue;
        std::vector<std::string> forms;
        bool anyTranslated = false;
        for (size_t form = 0; form < plural.formCount; ++form) {
            const char *text = plural.forms[form];
            forms.emplace_back(text == nullptr ? "" : text);
            if (text != nullptr && text[0] != '\0') anyTranslated = true;
        }
        if (anyTranslated) SelectedPlurals[plural.singular] = std::move(forms);
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

std::optional<std::string> firstLanguageIn(const char *value) {
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

const std::vector<LanguageInfo> &languages() {
    // English is not a catalogue -- it is the source text every msgid already
    // holds -- so it is listed here rather than shipped as a .po.
    static const std::vector<LanguageInfo> all = [] {
        std::vector<LanguageInfo> list{{"en", "English"}};
        for (size_t i = 0; i < kI18nCatalogCount; ++i) {
            list.push_back({kI18nCatalogs[i].code, kI18nCatalogs[i].nativeName});
        }
        return list;
    }();
    return all;
}

std::optional<std::string> parseLanguage(std::string_view value) {
    const std::string text = normalizedTag(value);
    if (text.empty()) return std::nullopt;
    for (const LanguageInfo &language : languages()) {
        if (normalizedTag(language.tag) == text) return std::string(language.tag);
    }
    // "pt_PT" finds "pt_BR" when Brazilian is the only Portuguese that ships,
    // which is better than falling all the way back to English.
    const std::string base = baseOf(text);
    for (const LanguageInfo &language : languages()) {
        if (baseOf(normalizedTag(language.tag)) == base) return std::string(language.tag);
    }
    return std::nullopt;
}

std::string systemLanguage() {
    for (const char *name : {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (const auto language = firstLanguageIn(std::getenv(name))) return *language;
    }
    return "en";
}

const char *nativeName(const std::string &tag) {
    for (const LanguageInfo &language : languages()) {
        if (normalizedTag(language.tag) == normalizedTag(tag)) return language.nativeName;
    }
    return "English";
}

void setLanguage(const std::string &tag) {
    const auto resolved = parseLanguage(tag);
    CurrentTag = resolved.value_or("en");
    loadInto(catalogFor(CurrentTag));
    Initialized = true;
    // Carried across exec: the installer and the watcher run in the language
    // the launcher chose, even before either reads settings.json.
    setenv("TUXBLOX_LANGUAGE", CurrentTag.c_str(), 1);
}

std::string current() {
    return CurrentTag;
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
    const auto found = SelectedStrings.find(source);
    return found != SelectedStrings.end() ? found->second.c_str() : source;
}

const char *trc(const char *context, const char *source) {
    if (context != nullptr && context[0] != '\0' && Initialized) {
        const auto byContext = SelectedContexts.find(context);
        if (byContext != SelectedContexts.end()) {
            const auto found = byContext->second.find(source);
            if (found != byContext->second.end()) return found->second.c_str();
        }
    }
    return tr(source);
}

std::string trf(const char *source, std::initializer_list<std::string> args) {
    return formatPlaceholders(tr(source), args);
}

std::string trn(const char *singular, const char *plural, long long count) {
    // English, and any language whose forms are missing, splits at one. Every
    // other language splits where its own catalogue says it does.
    const char *chosen = count == 1 ? singular : plural;
    if (Initialized && SelectedCatalog != nullptr && SelectedCatalog->pluralIndex != nullptr) {
        const auto found = SelectedPlurals.find(singular);
        if (found != SelectedPlurals.end()) {
            const unsigned long long magnitude = count < 0 ? static_cast<unsigned long long>(-count)
                                                           : static_cast<unsigned long long>(count);
            const size_t form = SelectedCatalog->pluralIndex(magnitude);
            if (form < found->second.size() && !found->second[form].empty()) {
                return formatPlaceholders(found->second[form], {std::to_string(count)});
            }
        }
    }
    return formatPlaceholders(tr(chosen), {std::to_string(count)});
}

std::string normalizeLanguageSetting(const std::string &value) {
    if (value.empty() || lowered(value) == "system") return "system";
    if (const auto language = parseLanguage(value)) return *language;
    return "system";
}

} // namespace tuxblox::i18n
