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
#include "app.h"

namespace tuxblox {

// The --headless counterpart to Ui::renderFrame's loop: polls `app` until it
// finishes and reports progress on the terminal. On a TTY that's a single
// bar redrawn in place; when stdout is redirected it's one plain line per
// step, so piping into a log stays readable.
//
// Installs a SIGINT handler for the duration of the call so Ctrl-C cancels
// through App::cancel() -- the same path the GUI's window-close uses, which
// is what cleans up a partial install tree. Errors go to stderr.
//
// Returns true if the install completed and the launcher is ready to exec.
bool runConsoleInstall(App& app);

} // namespace tuxblox
