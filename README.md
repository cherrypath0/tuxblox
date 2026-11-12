> [!WARNING]
> **Roblox Player support is still in progress!** TuxBlox currently supports Roblox Studio. Updates will be posted on our [Discord](https://tuxblox.net/discord).

<p align="center">
  <img src="https://static.tuxblox.net/images/banner/tuxblox-banner.png" width="600">
</p>

***

# TuxBlox 🐧

TuxBlox is a free, open-source compatibility layer and launcher that runs Roblox Studio on Linux. It's designed for both beginners and experts, and is under **active development**.

## How It Works

Similar to [Wine](https://www.winehq.org/), TuxBlox doesn't emulate Windows or use a virtual machine. It runs the official Windows version of Roblox on Linux by translating its Windows API calls at runtime.

TuxBlox maintains its own Wine build (based on Wine 11.0 and Proton) made specifically for Roblox, and bundles it. You don't need Wine installed. Targeting a single application lets us fix problems at the source, including upstream Wine bugs that affect Roblox, instead of working around them with configuration.

TuxBlox runs the **official, unmodified** Roblox binaries. It never patches or repackages them, and it refuses to run files that have been modified.

## Features

### Roblox Studio

* **Docking and panels** behave the same way they do on Windows.
* **WebView2 panels work**, through a Linux-side WebKitGTK runtime.
* **No random black windows or squares.**
* **Windows-accurate font rendering.**
* **Lower memory use** than standard Wine.
* **NTSYNC, fsync and esync** support.
* **Discord Rich Presence** and Roblox link handling.
* **Studio's MCP server** works.
* **File uploads** through your own file manager, and **`.rbxl` / `.rbxlx` files** open directly.

Known issues are listed in the [documentation](https://tuxblox.net/docs).

### Launcher

* **Works out of the box:** installation, updates and graphics setup are handled for you. There are no launch options to supply and no prefix to maintain.
* **Session manager:** see each running session's memory use, process ID, Roblox version and logs, and stop sessions.
* **Version picker:** download any Roblox version you choose, straight from Roblox's servers.
* **FastFlag manager.**
* **Release channels:** switch between stable, canary and experimental, with automatic updates.
* **Full control:** every default the launcher applies can be changed in its settings.

## System Requirements

### Hardware

* **Processor:** x86-64 with SSE3 support or newer (ARM64 is planned)
* **Memory:** 4 GB of RAM
* **Disk space:** 4 GB free
* **Graphics:** a GPU with working Vulkan drivers

### Software

* **Kernel:** Linux 6.7 or newer (6.14 or newer recommended for NTSYNC)
* **C library:** glibc 2.31 or newer
* **Display:** X11 or Wayland

TuxBlox installs and runs as a normal user. Root is never required.

## Installation

There are two ways to install TuxBlox. Both install to `~/.tuxblox`.

1. **Install script.** Open a terminal and run:

   ```bash
   curl -fsSL https://tuxblox.net/install.sh | bash
   ```

   You can [read the script](https://tuxblox.net/install.sh) before running it.

2. **Releases page.** Download the latest stable installer from [tuxblox.net/releases](https://tuxblox.net/releases) and run it.

To build TuxBlox yourself, see [Building From Source](https://tuxblox.net/docs/development/building-from-source).

## Privacy

Some features contact TuxBlox's own servers: self-updates, the optional server-location lookup for Team Create, and optional crash reports. Roblox itself is always downloaded from Roblox's servers, and your Roblox account cookie is never sent to TuxBlox. See our [privacy policy](https://tuxblox.net/privacy).

## FAQ

**Is TuxBlox a fork of Vinegar or Sober?**
No, TuxBlox is its own project. Vinegar runs the Windows client through the Wine you install yourself, while TuxBlox bundles its own compatibility layer built for Roblox. Sober runs the Android version of Roblox instead.

**Does it work on Wayland?**
Yes. The launcher runs natively on Wayland, and Studio runs through XWayland.

**Why isn't there a Flatpak or a distro package?**
The Flatpak sandbox gets in the way of controller access and system integration, and TuxBlox's self-update schedule doesn't fit package repositories.

**Can I run other Windows apps with it?**
No. TuxBlox only runs programs signed by Roblox or by TuxBlox itself.

**Is it free?**
Yes. TuxBlox is free and open source under GPLv3.

Additional questions are answered in [tuxblox.net/docs/getting-started/faq](https://tuxblox.net/docs/getting-started/faq).

## Contributing

Contributions are welcome! Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a pull request. If you use AI tools, also read [AGENTS.md](AGENTS.md) for the repository's AI policy.

To report a security vulnerability, **don't open a public issue**. See [SECURITY.md](SECURITY.md).

## Any Questions?

Join our [Discord server](https://tuxblox.net/discord) for support, development updates and discussion, or read the [documentation](https://tuxblox.net/docs).

## Legal & Disclaimer

- **TuxBlox is an independent, unofficial community project.** It is not affiliated with, authorized or endorsed by Roblox Corporation. Roblox is a trademark of Roblox Corporation.
- **TuxBlox's own code is licensed under GPLv3.**
- **This project includes a modified version of WineHQ's [Wine](https://gitlab.winehq.org/wine/wine) and parts of Valve's [Proton](https://github.com/ValveSoftware/Proton)**, which remain licensed under LGPLv2.1. TuxBlox's own code inside `compat/` (`compat/tuxblox/` and `compat/webkitgtk/bundle/`) is GPLv3.
- See [`third_party_licenses/`](third_party_licenses/) for the full licence text of all third-party code in this repository.
