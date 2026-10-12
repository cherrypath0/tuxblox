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

#include "launch/graphics.h"

namespace tuxblox {

GraphicsFiles graphicsFiles(const GraphicsConfig& graphics) {
    GraphicsFiles files;

    // Roblox picks its renderer at runtime and does not always land on Vulkan, so whichever Direct3D implementation is selected has to be installed and working even if a given session never touches it.
    if (graphics.useWineD3D) {
        files.wined3d = {"d3d12", "d3d11", "d3d10", "d3d10core", "d3d10_1", "d3d9"};
    } else {
        // No d3d12 here: vkd3d-proton is not shipped, so the prefix keeps the
        // builtin d3d12 it was created with. Roblox never asks for it anyway.
        files.dxvk = {"d3d11", "d3d10core", "d3d9"};
    }

    if (graphics.useDxvkDxgi) {
        files.dxvk.push_back("dxgi");
    } else {
        files.wined3d.push_back("dxgi");
    }

    files.nvapi = graphics.useNvapi;
    return files;
}

std::map<std::string, std::string> graphicsDllOverrides(const GraphicsConfig& graphics) {
    const GraphicsFiles files = graphicsFiles(graphics);
    std::map<std::string, std::string> overrides;

    // Only the DXVK libraries need naming. The builtin ones are what Wine reaches for on its own, so asking for them by name would say nothing.
    for (const std::string& name : files.dxvk) {
        overrides[name] = "n";
    }

    if (files.nvapi) {
        overrides["nvapi64"] = "n";
        overrides["nvapi"] = "n";
    }

    return overrides;
}

} // namespace tuxblox
