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

#include "desktop_integration.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace tuxblox;

    // Nothing is set yet, so TuxBlox takes it -- this is the ordinary first install.
    assert(shouldClaimAssociation("", false));
    assert(shouldClaimAssociation("", true));

    // Already TuxBlox's own: setting it again costs nothing and repairs a half-written mimeapps.list.
    assert(shouldClaimAssociation("tuxblox-player-handler.desktop", true));
    assert(shouldClaimAssociation("tuxblox-studio-handler.desktop", true));
    assert(shouldClaimAssociation("tuxblox-studio-place.desktop", true));

    // A development handler is put there deliberately and by hand, so it outranks the installed one.
    assert(!shouldClaimAssociation("tuxblox-player-dev.desktop", true));
    assert(!shouldClaimAssociation("tuxblox-studio-dev.desktop", true));

    // Another program the user chose stays chosen. Someone running Sober or Vinegar alongside TuxBlox
    // must not find their links quietly taken over every time the launcher starts.
    assert(!shouldClaimAssociation("org.vinegarhq.Sober.desktop", true));
    assert(!shouldClaimAssociation("org.vinegarhq.Vinegar.desktop", true));
    assert(!shouldClaimAssociation("firefox.desktop", true));

    // A default naming a program that is not installed opens nothing at all, so there is no choice
    // there to respect -- it is a leftover, and TuxBlox takes the type over.
    assert(shouldClaimAssociation("org.vinegarhq.Vinegar.desktop", false));
    assert(shouldClaimAssociation("some-removed-app.desktop", false));

    // A development handler that is no longer installed is a leftover like any other.
    assert(shouldClaimAssociation("tuxblox-player-dev.desktop", false));

    // Only an entry for the exact type counts as somebody's choice.
    {
        const std::string text =
            "[Added Associations]\n"
            "application/x-roblox-place=someone-else.desktop;\n"
            "\n"
            "[Default Applications]\n"
            "x-scheme-handler/roblox=other-roblox.desktop\n"
            "application/x-roblox-place=tuxblox-studio-place.desktop;fallback.desktop;\n"
            "text/xml=chromium.desktop\n";

        assert(explicitDefaultFor(text, "x-scheme-handler/roblox") == "other-roblox.desktop");
        // A list names a first choice, and that is the one that opens the file.
        assert(explicitDefaultFor(text, "application/x-roblox-place") == "tuxblox-studio-place.desktop");

        // Nothing is set for the XML place type here. It is a kind of XML, so the desktop would answer
        // with whatever opens XML -- but nobody chose that for Roblox files, so TuxBlox may claim it.
        assert(explicitDefaultFor(text, "application/x-roblox-place+xml").empty());
        assert(explicitDefaultFor(text, "application/x-roblox-model").empty());

        // Only the defaults section counts: an added association is not a default.
        assert(explicitDefaultFor("[Added Associations]\napplication/x-roblox-place=a.desktop\n",
                                  "application/x-roblox-place")
                   .empty());

        // A type whose name is a prefix of another must not pick up that other one's entry.
        assert(explicitDefaultFor("[Default Applications]\napplication/x-roblox-place+xml=a.desktop\n",
                                  "application/x-roblox-place")
                   .empty());
    }

    // Nothing sensible in the file is answered rather than guessed at.
    assert(explicitDefaultFor("", "application/x-roblox-place").empty());
    assert(explicitDefaultFor("not an ini file at all", "application/x-roblox-place").empty());
    assert(explicitDefaultFor("[Default Applications]\napplication/x-roblox-place=\n",
                              "application/x-roblox-place")
               .empty());

    printf("desktop_integration: all tests passed\n");
    return 0;
}
