> [!WARNING]
> **TuxBlox is still early in development.** Updates are posted on our Discord: [discord.gg/QkgBMMMwdW](https://tuxblox.net/discord)

***

![TuxBlox Banner](https://assetdelivery.tuxblox.net/images/png/banner/tuxblox-banner.png)

# TuxBlox 🐧
TuxBlox is an open-source project that runs the official Roblox Client and Studio on Linux using a modified build of Proton.

## Getting Started
Full documentation is currently a work in progress. You can find documentation at [tuxblox.net/docs](https://tuxblox.net/docs).

## Features
* **Easy Setup:** You only download the installer, we do the rest. Updates happen in the background.
* **Infinite Customization**: Customize Proton flags, environment flags, or even specify a custom Proton build if you have one.
* **Browser Integration**: Run `./install-handler.sh` to register TuxBlox as the handler for `roblox-player:`/`roblox-studio:`/`roblox-studio-auth:` links, so "Play"/"Edit" on roblox.com launches TuxBlox directly.

## System Requirements
### Hardware Requirements:
* **Processor:** 64-bit x86 (x86_64) Architecture with SSE3 Support
* **Storage Space:** 3 GB or higher
### Software Requirements:
* **Operating System:** Any major 64-bit Linux distribution
* **Kernel Version:** 5.0 or higher
### Required Packages (only necessary if you want to compile it yourself):
* curl
* nuitka
* python
* podman/docker

## Legal & Disclaimer
- **TuxBlox is an independent, open-source project.** It is not affiliated with, authorized, or endorsed by Roblox Corporation. 
- **This project is licensed under the GPLv3 License.**
- **This project includes a modified build of Proton, which remains licensed under LGPLv2.1**. 
- See [`third_party_licenses/`](third_party_licenses/) for the full license text of Proton and any other bundled components.