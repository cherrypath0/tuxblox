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
#include <string>

namespace tuxblox {

// Publishes Roblox to the Linux app menu, routed through this launcher.
//
// TuxBlox's Proton redirects every winemenubuilder menu entry into the prefix's
// own c:\proton_shortcuts (winemenubuilder.c:1262) instead of the host's
// applications directory, so nothing on Linux ever sees them. This reads those
// entries and re-emits them as real .desktop files whose Exec= goes through the
// launcher ("--run-exe"), with a "via TuxBlox" comment -- plan/todo.md item 2,
// option 1.
//
// Only the two Roblox executables are exported; anything else that happens to
// be in the prefix is skipped rather than published to the user's app menu.
// Best-effort throughout: never throws, and a failure just means no menu entry.
void exportPrefixShortcuts(const std::string& installDir, const std::string& launcherExePath);

// Same, against explicit output directories. Exposed for tests.
void exportPrefixShortcutsTo(const std::string& installDir, const std::string& launcherExePath,
                             const std::string& appsDir, const std::string& iconsDir);

// Testable seams.

// winemenubuilder's escape() doubles backslashes, and the desktop-entry spec's
// own escaping doubles them again, so each separator arrives as four literal
// backslashes. Collapses runs of four back to one.
std::string unescapeWinemenubuilderPath(const std::string& escaped);

// Pulls the quoted executable path out of a proton_shortcuts Exec= value and
// unescapes it. Returns "" if the value isn't the expected shape.
std::string exeFromDesktopExecLine(const std::string& execValue);

// Pulls the same quoted substring but leaves it exactly as written (still in
// winemenubuilder's four-backslash form). This is what must be written back
// into a new Exec= line -- see exportPrefixShortcutsTo()'s use of it for why.
std::string quotedExecValue(const std::string& execValue);

} // namespace tuxblox
