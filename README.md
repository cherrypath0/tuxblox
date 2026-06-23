> [!WARNING]
> **TuxBlox is distributed ONLY through [tuxblox.net](https://tuxblox.net) and this official GitHub repository.** Files downloaded from any other source are not provided by us and may contain viruses or malware. Always verify the URL before running any installation scripts.

> [!WARNING]
> **Account safety is not guaranteed.** TuxBlox is an unofficial, unaffiliated compatibility layer. Anti-cheat detection behavior can change at any time as Roblox's systems evolve, and an account that works today may be flagged or banned tomorrow. Use at your own risk.

***

![TuxBlox Banner](assets/images/png/banner/tuxblox-banner-plain-small.png)

# TuxBlox 🐧
TuxBlox is an open-source project that makes the Roblox macOS Game Client compatible on Linux by translating macOS calls to Linux using a compatibility layer.

## How It Works
TuxBlox translates the macOS client files and frameworks directly into native calls Linux can understand.

## Key Features
* **Translation Layer:** TuxBlox translates macOS system calls to Linux on the fly, running the official Roblox client directly.
* **Lightweight by Design:** TuxBlox runs right alongside your existing setup without bloated processes or background services hogging your RAM.
* **Vulkan Graphics Bridge:** TuxBlox intercepts Apple's graphics calls and hands them straight to Linux Vulkan drivers for maximum frame rates.
* **Automatic Updates:** Install it once and forget it. The built-in updater handles client updates seamlessly in the background.
* **Built for Speed:** Written in C++ to keep system overhead minimal, ensuring your hardware focuses entirely on the game.

## System Requirements
### Hardware Requirements
* **Processor:** Intel or AMD, 64-bit x86 (x86_64) Architecture with SSE3 Support
* **Memory:** 4 GB
* **Storage:** 1 GB

### Software Requirements
* **Operating System:** Any major 64-bit Linux distribution
* **Kernel Version:** 5.0 or higher
* **Filesystem:** Unencrypted (because the compatibility layer relies on methods that require an unencrypted filesystem)

## Getting Started
Full documentation is currently a work in progress. You can find the available setup guides at [tuxblox.net/docs](https://tuxblox.net/docs) once its available.

## Disclaimer
TuxBlox is an independent compatibility tool intended for personal use. It is not affiliated or associated with Roblox Corporation in any way.

## License
This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for the full text.