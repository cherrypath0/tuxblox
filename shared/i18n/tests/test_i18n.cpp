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

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

int main() {
    using namespace tuxblox::i18n;

    // Before initialization the source string is the answer, so tests and
    // early callers never see a language they did not ask for.
    assert(std::string(tr("Settings")) == "Settings");

    setLanguage(Language::PortugueseBrazil);
    assert(std::string(tr("Settings")) == "Configurações");
    assert(std::string(tr("Home")) == "Início");
    assert(std::string(tr("Stopped {0}")) == "Parou {0}");
    assert(trf("Stopped {0}", {"3"}) == "Parou 3");
    assert(trn("{} entry", "{} entries", 0) == "0 entradas");
    assert(trn("{} entry", "{} entries", 1) == "1 entrada");
    assert(trn("{} entry", "{} entries", 2) == "2 entradas");
    assert(std::string(tr("__missing__")) == "__missing__");

    setLanguage(Language::Spanish);
    assert(std::string(tr("Settings")) == "Configuración");
    assert(std::string(tr("Home")) == "Inicio");
    assert(trf("Stopped {0}", {"2"}) == "Detuvo 2");

    setLanguage(Language::English);
    assert(std::string(tr("Settings")) == "Settings");

    assert(parseLanguage("pt-BR") == Language::PortugueseBrazil);
    assert(parseLanguage("pt") == Language::PortugueseBrazil);
    assert(parseLanguage("es_MX") == Language::Spanish);
    assert(parseLanguage("en_US") == Language::English);
    assert(parseLanguage("fr") == std::nullopt);
    assert(tag(Language::PortugueseBrazil) == "pt_BR");
    assert(normalizeLanguageSetting("pt-BR") == "pt_BR");
    assert(normalizeLanguageSetting("es_MX") == "es");
    assert(normalizeLanguageSetting("nonsense") == "system");

    // LANGUAGE is a priority list, then the usual locale variables.
    setenv("LANGUAGE", "pt_BR:en", 1);
    unsetenv("LC_ALL");
    unsetenv("LC_MESSAGES");
    unsetenv("LANG");
    assert(systemLanguage() == Language::PortugueseBrazil);
    setenv("LANGUAGE", "fr:es", 1);
    assert(systemLanguage() == Language::Spanish);
    unsetenv("LANGUAGE");
    setenv("LANG", "es_ES.UTF-8", 1);
    assert(systemLanguage() == Language::Spanish);
    setenv("LANG", "C", 1);
    assert(systemLanguage() == Language::English);

    // TUXBLOX_LANGUAGE wins over the setting, which is how the launcher hands
    // its choice to the installer and the watcher.
    setenv("TUXBLOX_LANGUAGE", "pt_BR", 1);
    initializeFromSetting("es");
    assert(current() == Language::PortugueseBrazil);
    unsetenv("TUXBLOX_LANGUAGE");
    initializeFromSetting("es");
    assert(current() == Language::Spanish);

    printf("i18n: all tests passed\n");
    return 0;
}
