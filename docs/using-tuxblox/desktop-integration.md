# Desktop Integration

TuxBlox wires itself into your desktop so Roblox behaves like any other application: it shows up in your applications menu, `roblox:` links open in it, and double clicking a place file opens Studio.

All of this is set up for you during install and kept up to date every time the launcher starts. You should not need to do anything on this page.

## What appears in your applications menu

| Entry | When it appears |
|---|---|
| **TuxBlox** | Immediately after install |
| **Roblox Studio** (via TuxBlox) | After Studio has been installed and run once |
| **Roblox Player** (via TuxBlox) | After Player has been installed and run once |

The Roblox entries come from Roblox's own Start Menu shortcuts inside the virtual drive. TuxBlox reads them and republishes them as proper Linux entries, so they carry Roblox's own icons and names. They are routed back through the launcher, which is what lets them keep working after Roblox updates itself.

The **TuxBlox** entry also has a right click action for opening this documentation.

## Links that open in TuxBlox

TuxBlox claims these URL schemes:

| Scheme | Opens |
|---|---|
| `roblox:` | Roblox Player |
| `roblox-player:` | Roblox Player |
| `roblox-studio:` | Roblox Studio |
| `roblox-studio-auth:` | Roblox Studio, for signing in from the website |

This is what makes the **Play** button on the Roblox website, and the **Edit in Studio** button on the Creator Hub, work from your browser.

When one of these links is opened, TuxBlox starts the right application directly. The launcher window does not appear.

> [!NOTE]
> Firefox keeps its own separate list of protocol handlers. If Firefox keeps asking you what to open `roblox:` links with, or picks the wrong thing, that is Firefox's list rather than your desktop's. Firefox has to be closed to edit it, from Settings, then General, then Applications.

## Place files

`.rbxl` and `.rbxlx` files are registered to open in Roblox Studio.

Double clicking one in your file manager opens Studio with that place loaded, wherever the file happens to be on your disk. You do not have to move it into the virtual drive first, TuxBlox makes it reachable from inside.

## Where all this is written

Standard XDG locations in your home folder, nothing system wide:

| Path | What is there |
|---|---|
| `~/.local/share/applications/` | The `.desktop` entries |
| `~/.local/share/icons/hicolor/` | The TuxBlox and Roblox icons |
| `~/.local/share/mime/packages/` | The `.rbxl` and `.rbxlx` file type definitions |

Uninstalling TuxBlox removes all of them.

## If the icons or entries are missing

Start the launcher once and let it sit for a few seconds. It rewrites every entry on startup and refreshes your desktop's icon and MIME caches, which usually fixes it on its own.

If entries still do not appear, your desktop may be caching aggressively. Logging out and back in forces a rescan.

## Inside a Distrobox container

If TuxBlox is installed inside a Distrobox container, it also exports its entries to the host system so they appear in the host's applications menu rather than being trapped inside the container.

TuxBlox checks on startup whether the container has access to your graphics card and warns you if it does not. See [Common Problems](../troubleshooting/common-problems.md#distrobox-gpu-passthrough-warning) for what to do about that.
