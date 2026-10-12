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

#include <cassert>
#include <cstdio>

using namespace tuxblox;

static std::string joined(const std::map<std::string, std::string>& overrides) {
    std::string out;
    for (const auto& [dll, setting] : overrides) {
        out += (out.empty() ? "" : ";") + dll + "=" + setting;
    }
    return out;
}

int main() {
    // The default: Roblox runs on DXVK, which only happens if the launch asks for it by name.
    const std::string dxvkDefault = "d3d10core=n;d3d11=n;d3d9=n;dxgi=n;nvapi=n;nvapi64=n";
    assert(joined(graphicsDllOverrides(GraphicsConfig{})) == dxvkDefault);

    // The bug this pins: a second Roblox joins a drive it did not set up, so it never runs the step that lays the DXVK files down. The preferences are not part of that step and must still be there, or the session renders through the builtin Direct3D and loses most of its frames.
    GraphicsConfig joiningLaunch;
    assert(joined(graphicsDllOverrides(joiningLaunch)) == dxvkDefault);
    assert(!graphicsDllOverrides(joiningLaunch).count("wined3d"));

    // wined3d is what Wine reaches for unprompted, so asking for it by name would say nothing.
    GraphicsConfig wined3d;
    wined3d.useWineD3D = true;
    wined3d.useDxvkDxgi = false;
    assert(joined(graphicsDllOverrides(wined3d)) == "nvapi=n;nvapi64=n");

    GraphicsFiles files = graphicsFiles(wined3d);
    assert(files.dxvk.empty());
    assert((files.wined3d == std::vector<std::string>{"d3d12", "d3d11", "d3d10", "d3d10core",
                                                      "d3d10_1", "d3d9", "dxgi"}));

    // A hand-written dxgi override keeps DXVK for Direct3D and hands dxgi back to Wine.
    GraphicsConfig ownDxgi;
    ownDxgi.useDxvkDxgi = false;
    assert(joined(graphicsDllOverrides(ownDxgi)) == "d3d10core=n;d3d11=n;d3d9=n;nvapi=n;nvapi64=n");

    files = graphicsFiles(ownDxgi);
    assert((files.dxvk == std::vector<std::string>{"d3d11", "d3d10core", "d3d9"}));
    assert((files.wined3d == std::vector<std::string>{"dxgi"}));

    // Without NVAPI nothing names it, and the files step takes that as "remove it from the drive".
    GraphicsConfig noNvapi;
    noNvapi.useNvapi = false;
    assert(joined(graphicsDllOverrides(noNvapi)) == "d3d10core=n;d3d11=n;d3d9=n;dxgi=n");
    assert(!graphicsFiles(noNvapi).nvapi);

    std::printf("test_graphics: all passed\n");
    return 0;
}
