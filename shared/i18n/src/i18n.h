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
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tuxblox::i18n {

// A language this build carries. The tag is what settings.json stores and what
// TUXBLOX_LANGUAGE passes to a child process.
struct LanguageInfo {
    const char *tag = "en";
    // Written in the language itself, so somebody who cannot read the current
    // one can still find theirs in the list.
    const char *nativeName = "English";
};

// Every language offered, English first and the catalogues after it. The set
// comes from the .po files that were present at build time, so a new language
// is one file and no C++.
const std::vector<LanguageInfo> &languages();

// The language tag a desktop locale asks for: an exact match first, then the
// base language, so "pt_PT" finds "pt_BR" when that is the only Portuguese
// this build carries. Accepts "pt-BR" and "es_MX.UTF-8@euro" alike. Returns
// nullopt when nothing matches, which is what makes an unknown tag fall back
// rather than show blank text.
std::optional<std::string> parseLanguage(std::string_view tag);

// What the desktop asks for, from LANGUAGE, LC_ALL, LC_MESSAGES and LANG, in
// that order. English when none of them names a language this build carries.
std::string systemLanguage();

// The language's own name, for the picker: never translated.
const char *nativeName(const std::string &tag);

// Before any initialize* call, tr() returns the English source string. Every
// executable entry point must initialize before constructing UI.
void initializeFromSetting(const std::string &setting);
void initializeFromSettingsFile(const std::string &settingsJsonPath);
void ensureInitialized();
void setLanguage(const std::string &tag);
std::string current();

// Looks the source string up in the selected catalogue and falls back to the
// source itself, which is the English text. The returned pointer is stable for
// the lifetime of the process after setLanguage().
const char *tr(const char *source);
const char *trc(const char *context, const char *source);

// Replaces {0}, {1}, ... and a single {} with the corresponding argument.
std::string trf(const char *source, std::initializer_list<std::string> args);

// Picks the form the catalogue's own Plural-Forms rule asks for, which is not
// always "one or many", then substitutes {} with the decimal count.
std::string trn(const char *singular, const char *plural, long long count);

// Canonical settings.json value: "system", or a tag some catalogue carries.
// Unknown values become "system", so a hand-edited file can never prevent
// startup.
std::string normalizeLanguageSetting(const std::string &value);

} // namespace tuxblox::i18n
