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

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

bool offers(const std::string &tag) {
    const auto &all = tuxblox::i18n::languages();
    return std::any_of(all.begin(), all.end(),
                       [&](const tuxblox::i18n::LanguageInfo &language) { return tag == language.tag; });
}

} // namespace

int main() {
    using namespace tuxblox::i18n;

    // Before initialization the source string is the answer, so tests and
    // early callers never see a language they did not ask for.
    assert(std::string(tr("Settings")) == "Settings");

    // English is always offered even though it ships no catalogue: the msgids
    // are the English text.
    assert(offers("en"));
    assert(offers("pt_BR"));
    assert(offers("es"));
    // de.po exists but has nothing in it yet, so it is not offered as a
    // language that would render entirely in English.
    assert(!offers("de"));
    assert(std::string(nativeName("pt_BR")) == "Português (Brasil)");

    setLanguage("pt_BR");
    assert(current() == "pt_BR");
    assert(std::string(tr("Settings")) == "Configurações");
    assert(std::string(tr("Home")) == "Início");
    assert(std::string(tr("Stopped {0}")) == "Parou {0}");
    assert(trf("Stopped {0}", {"3"}) == "Parou 3");
    // Brazilian Portuguese puts zero with the singular, which is what its own
    // Plural-Forms rule says and English's rule does not.
    assert(trn("{} entry", "{} entries", 0) == "0 entrada");
    assert(trn("{} entry", "{} entries", 1) == "1 entrada");
    assert(trn("{} entry", "{} entries", 2) == "2 entradas");
    assert(std::string(tr("__missing__")) == "__missing__");

    setLanguage("es");
    assert(std::string(tr("Settings")) == "Configuración");
    assert(std::string(tr("Home")) == "Inicio");
    assert(trf("Stopped {0}", {"2"}) == "Detuvo 2");
    assert(trn("{} entry", "{} entries", 0) == "0 entradas");
    assert(trn("{} entry", "{} entries", 1) == "1 entrada");

    setLanguage("en");
    assert(std::string(tr("Settings")) == "Settings");
    assert(trn("{} entry", "{} entries", 1) == "1 entry");
    assert(trn("{} entry", "{} entries", 5) == "5 entries");

    assert(parseLanguage("pt-BR") == "pt_BR");
    // Only one Portuguese ships, so a bare "pt" and even "pt_PT" reach it
    // rather than falling all the way back to English.
    assert(parseLanguage("pt") == "pt_BR");
    assert(parseLanguage("pt_PT") == "pt_BR");
    assert(parseLanguage("es_MX") == "es");
    assert(parseLanguage("es_ES.UTF-8@euro") == "es");
    assert(parseLanguage("en_US") == "en");
    assert(parseLanguage("fr") == std::nullopt);
    assert(normalizeLanguageSetting("pt-BR") == "pt_BR");
    assert(normalizeLanguageSetting("es_MX") == "es");
    assert(normalizeLanguageSetting("nonsense") == "system");
    assert(normalizeLanguageSetting("") == "system");

    // LANGUAGE is a priority list, then the usual locale variables.
    setenv("LANGUAGE", "pt_BR:en", 1);
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    unsetenv("LANG");
    assert(systemLanguage() == "pt_BR");
    setenv("LANGUAGE", "fr:es", 1);
    assert(systemLanguage() == "es");
    unsetenv("LANGUAGE");
    setenv("LANG", "es_ES.UTF-8", 1);
    assert(systemLanguage() == "es");
    setenv("LANG", "C", 1);
    assert(systemLanguage() == "en");

    // TUXBLOX_LANGUAGE wins over the setting, which is how the launcher hands
    // its choice to the installer and the watcher.
    setenv("TUXBLOX_LANGUAGE", "pt_BR", 1);
    initializeFromSetting("es");
    assert(current() == "pt_BR");
    unsetenv("TUXBLOX_LANGUAGE");
    initializeFromSetting("es");
    assert(current() == "es");
    // An unknown setting is the desktop's choice, not blank text. The
    // environment variable is cleared first because setLanguage() exports it.
    unsetenv("TUXBLOX_LANGUAGE");
    initializeFromSetting("nonsense");
    assert(current() == "en");

    printf("i18n: all tests passed\n");
    return 0;
}
