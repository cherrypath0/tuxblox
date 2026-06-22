> [!WARNING]
> **TuxBlox is exclusively distributed through [tuxblox.net](https://tuxblox.net) and this official GitHub repository.** Files downloaded from any other source are not provided by us and may contain viruses or malware. Always verify the URL before running any installation scripts.

***

![TuxBlox Logo](assets/icons/tuxblox-50.png)

# TuxBlox 🐧
TuxBlox lets you run the native Windows Roblox client on Linux by using a lightweight, stripped-down Windows virtual machine. It skips Wine and compatibility layers entirely to give you a native experience.

## How it Works
Instead of dealing with compatibility layers, TuxBlox utilizes KVM and QEMU to launch a heavily debloated Windows environment. This setup boots straight into Roblox and nothing else. Because there is no desktop, browser, or background software, the VM devotes all its resources strictly to the game.

## Key Features
* **Native Execution:** Runs the official Roblox executable without Wine.
* **Direct Booting:** Launches straight into the game with no Windows desktop clutter.
* **GPU Passthrough:** Supports VFIO passthrough to deliver native graphics performance.
* **Snapshot Recovery:** Allows you to roll back the VM state instantly if an update breaks something.

## System Requirements
### Hardware Specifications
* **Memory:** 6GB RAM minimum (8GB recommended)
* **Storage:** 60GB available space (70GB recommended)
* **Processor:** 2 CPU cores minimum (4 CPU cores recommended)
* **Motherboard:** CPU with IOMMU support (Intel VT-d or AMD-Vi enabled in BIOS)
* **Graphics:** A dedicated GPU compatible with VFIO passthrough (an integrated or second GPU is required for the Linux host during passthrough)

### Software Dependencies
* **OS:** A Linux distribution with KVM and QEMU configured

## Getting Started
Full documentation is currently a work in progress. You can find the available setup guides inside the `docs/` folder.

## Disclaimer
TuxBlox is an independent compatibility tool intended for personal use. It is not affiliated with, authorized by, or associated with Roblox Corporation or Microsoft.

## License
This project is licensed under the MIT License. See the [LICENSE](./LICENSE) file for the full text.