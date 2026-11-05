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

#pragma once
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace tuxblox::i18n {

enum class Language { English, PortugueseBrazil, Spanish };

// Parses "en", "en_US", "pt", "pt-BR", "pt_BR", "es", "es_MX", and ignores
// an encoding or @modifier suffix. Returns nullopt for anything else.
std::optional<Language> parseLanguage(std::string_view tag);

// The language the desktop asks for, from LANGUAGE, LC_ALL, LC_MESSAGES and
// LANG, in that order. Defaults to English when none of them names a language
// this build carries.
Language systemLanguage();

std::string tag(Language language);
const char* nativeName(Language language);

// Before any initialize* call, tr() returns the English source string. Every
// executable entry point must initialize before constructing UI.
void initializeFromSetting(const std::string& setting);
void initializeFromSettingsFile(const std::string& settingsJsonPath);
void ensureInitialized();
void setLanguage(Language language);
Language current();

// Looks the source string up in the selected catalog, then in English, then
// falls back to the source itself. The returned pointer is stable for the
// lifetime of the process after setLanguage().
const char* tr(const char* source);
const char* trc(const char* context, const char* source);

// Replaces {0}, {1}, ... and a single {} with the corresponding argument.
std::string trf(const char* source, std::initializer_list<std::string> args);

// Picks the singular form when count is 1 and the plural form otherwise, then
// substitutes {} with the decimal count.
std::string trn(const char* singular, const char* plural, long long count);

// Canonical settings.json value: "system", "en", "pt_BR" or "es". Unknown
// values become "system", so a hand-edited file can never prevent startup.
std::string normalizeLanguageSetting(const std::string& value);

} // namespace tuxblox::i18n
