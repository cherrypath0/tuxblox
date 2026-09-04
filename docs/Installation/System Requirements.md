# System Requirements

TuxBlox has several system requirements to ensure that Roblox can run properly on Linux. Please make sure your system meets these requirements before installing TuxBlox.

### Hardware Requirements

* **Processor:** x86-64 processor with SSE4.1 support or newer
* **Storage:** 6 GB or more of available storage space
* **RAM:** 8 GB or more

### Software Requirements

* **Operating System:** Ubuntu 20.04 LTS, Debian 11 Bullseye, Fedora 32, Arch Linux, or newer, with glibc 2.31 or newer
* **Kernel:** Linux 6.14 or newer
* **GPU Drivers:** NVIDIA Proprietary 418.49.04, AMD Mesa 17.0, or newer

Other Linux distributions may also work as long as they meet the requirements above.

Currently, WSL (Windows Subsystem for Linux) and virtual machines are not supported with TuxBlox. TuxBlox is designed to run directly on Linux systems.

## Unsupported Distros

TuxBlox has a few known unsupported distributions and configurations.

### Alpine Linux

Alpine Linux uses the musl C library instead of glibc. TuxBlox is built against glibc 2.31 or newer, so Alpine Linux is currently unsupported.

More generally, distributions that do not provide glibc 2.31 or newer are not supported.

### Void Linux with musl

Void Linux provides both glibc and musl variants. The musl variant is not supported because TuxBlox requires glibc 2.31 or newer.

The glibc variant of Void Linux may be used as long as it meets the other system requirements.
