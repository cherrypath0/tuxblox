# System Requirements

Make sure your system meets these requirements before installing TuxBlox.

## Hardware

- **Processor:** x86-64 processor with SSE4.1 support or newer
- **Storage:** 6 GB or more of free space
- **Memory:** 8 GB of RAM or more

## Software

- **Operating System:** Ubuntu 20.04 or newer, Debian 11 or newer, Fedora 32 or newer, Arch Linux, or another distribution with glibc 2.31 or newer
- **Kernel:** Linux 6.14 or newer
- **GPU Drivers:** NVIDIA proprietary driver 418.49.04 or newer, or Mesa 17.0 or newer for AMD and Intel

Other distributions not listed above may still work, as long as they meet the requirements.

TuxBlox is designed to run directly on Linux. It does not currently support WSL (Windows Subsystem for Linux) or virtual machines.

## Distributions that will not work

### Alpine Linux

Alpine Linux uses musl instead of glibc. TuxBlox needs glibc 2.31 or newer, so Alpine Linux is not supported.

This applies to any distribution that does not provide glibc 2.31 or newer.

### Void Linux (musl variant)

Void Linux is offered with either glibc or musl. The musl version is not supported for the same reason as Alpine. The glibc version of Void Linux works fine, as long as it also meets the other requirements above.
