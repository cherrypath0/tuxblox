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
#include "cli.h"
#include <string>

namespace tuxblox {

// Runs the graphical install to completion. 0 means the install finished and the caller should hand off to the launcher, 1 means it failed, 2 means the window was closed first.
int runAdwInstall(App& app, const CliOptions& options);

// Shows the result of an uninstall and returns once it is dismissed.
int runAdwUninstallResult(bool ok);

// Shows a failure message in a window and returns once it is dismissed.
int runAdwError(const std::string& message);

} // namespace tuxblox
