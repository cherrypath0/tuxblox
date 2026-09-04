> [!WARNING]
> **Roblox Player support is highly experimental!** Except it to have bugs and issues. Updates will be posted on our Discord.

<p align="center">
  <img src="https://static.tuxblox.net/images/banner/tuxblox-banner.png" width="600">
</p>

***

# TuxBlox 🐧
TuxBlox is a new compatibility layer and launcher capable of running Roblox Studio on Linux, designed for both beginners and experts, and under **active development**.

## How It Works
Similar to [Wine](https://www.winehq.org/), instead of emulating Windows or using a virtual machine, TuxBlox automatically translates the Windows version of Roblox to Linux at runtime. 
TuxBlox maintains its own Wine build specifically optimized for Roblox. This allows us to make changes that cannot be achieved through configuration alone, including fixing upstream Wine bugs that affect Roblox.

## Features
* **Works out of the box**: Installation, updates and graphics setup are handled by the launcher. There are no launch options to supply or no prefix to maintain, TuxBlox handles everything for you.
* **Native performance**: No virtual machine sits between Roblox and your hardware. Studio starts quickly, and the GPU is driven directly rather than through an emulation layer.
* **Full control**: Every default the launcher applies can be overridden from its settings tab. Sensible behaviour out of the box, full control when you want it.

## System Requirements
### Hardware Requirements:
* **Processor:** x86-64 architecture with SSE3 support or newer
* **Disk Space:** 4 GB free
* **Memory:** 4 GB or more
### Software Requirements:
* **Operating System:** Ubuntu 20.04 LTS, Debian 11 Bullseye, Fedora 32, Arch Linux, or newer, with glibc 2.31+ support
* **Kernel Version:** Linux 6.14 or newer
* **Graphics Driver:** NVIDIA Proprietary 418.49.04, AMD Mesa 17.0, or newer

## Installation
There are 2 ways to download and install TuxBlox.
1. Installation Script
    - Open a terminal
    - Run: `curl -fsSL https://tuxblox.net/install.sh | sh`
2. Releases Page
    - Go over to our releases page [tuxblox.net/releases](https://tuxblox.net/releases)
    - Download the latest stable TuxBloxInstaller
    - After downloading, just run the installer

## Any Questions?
Feel free to join our [Discord server](https://discord.gg/tfdR4jU4kp) for support, development updates, and discussion.

## Legal & Disclaimer
- **TuxBlox is an independent, open-source project.** It is not affiliated with, authorized, or endorsed by Roblox Corporation. 
- **This project is licensed under the GPLv3 License.**
- **This project includes a modified version of WineHQ's [Wine](https://gitlab.winehq.org/wine/wine) and a portion of Valve's [Proton](https://github.com/ValveSoftware/Proton), which remains licensed under LGPLv2.1**. 
- See [`third_party_licenses/`](third_party_licenses/) for the full license text of any third party code within this repository.