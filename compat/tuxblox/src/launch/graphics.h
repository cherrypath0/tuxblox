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
#include <map>
#include <string>
#include <vector>

namespace tuxblox {

// Which graphics backend the prefix should be set up for. Roblox usually
// renders through its own Vulkan backend, but it can fall back to Direct3D or
// OpenGL, so both paths have to keep working.
struct GraphicsConfig {
    // Use Wine's OpenGL-based wined3d instead of Vulkan-based DXVK.
    bool useWineD3D = false;
    // Let DXVK provide dxgi. Turned off when wined3d is in use, or when the
    // user overrode dxgi by hand.
    bool useDxvkDxgi = true;
    // Disable d3d11 entirely.
    bool noD3D11 = false;
    // Provide NVAPI to the Direct3D path on NVIDIA hardware.
    bool useNvapi = true;
};

// Which Direct3D libraries a launch runs on. Worked out from the configuration alone, so a launch that joins a drive somebody else set up reaches the same answer as the one that set the drive up.
struct GraphicsFiles {
    // Laid into the drive from the layer's own DXVK.
    std::vector<std::string> dxvk;
    // Laid into the drive from the template drive's builtin Direct3D.
    std::vector<std::string> wined3d;
    // Direct3D's NVIDIA interface, which DXVK answers.
    bool nvapi = false;
};

GraphicsFiles graphicsFiles(const GraphicsConfig& graphics);

// The library preferences that make Roblox actually use what graphicsFiles() chose, which is a per-launch decision and not drive state: the files are already in the drive, but without these every launch loads the builtin Direct3D instead.
std::map<std::string, std::string> graphicsDllOverrides(const GraphicsConfig& graphics);

} // namespace tuxblox
