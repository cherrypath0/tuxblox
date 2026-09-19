# System Requirements

Most computers built in the last ten years will run TuxBlox. Here is the short version, followed by the details.

## Hardware

| | Minimum |
|---|---|
| **Processor** | x86-64 with SSE3 or newer |
| **Memory** | 8 GB of RAM |
| **Storage** | 6 GB free |
| **Graphics** | Anything with a working Vulkan driver |

ARM processors (including Raspberry Pi and Apple Silicon under Asahi Linux) are not supported.

## Software

| | Minimum |
|---|---|
| **Kernel** | Linux 6.7 or newer (6.14 recommended) |
| **C library** | glibc 2.31 or newer |
| **NVIDIA drivers** | Proprietary driver 418.49.04 or newer |
| **AMD and Intel drivers** | Mesa 17.0 or newer |

If you are not sure what any of that means, the distribution list below is an easier way to check.

Roblox will not start correctly below Linux 6.7: TuxBlox needs a way to ask the kernel which pages of memory a program has written to, and older kernels make it work that out the slow way instead, which Roblox's own protection notices and stops for.

Linux 6.14 is not required, but it is worth having. It is the first kernel that can handle Windows' way of making threads wait for each other directly, which is a large speed difference in Roblox. Without it TuxBlox uses the next best method available, and everything still works.

## Distributions

These are known to meet the requirements as long as they are reasonably up to date:

- Ubuntu 20.04 or newer
- Debian 11 or newer
- Fedora 32 or newer
- Arch Linux, Manjaro, EndeavourOS
- Linux Mint, Pop!\_OS
- openSUSE Tumbleweed

Other distributions will very likely work too. The list is not exhaustive, it is just the set we hear about most often.

> [!NOTE]
> The kernel requirement is the one that trips people up most. Long term support distributions sometimes ship an older kernel than their release date suggests. Check yours with `uname -r`.

## What will not work

### musl based distributions

TuxBlox needs glibc. Distributions built on musl instead cannot run it at all, and this is not something a workaround can fix.

That rules out:

- **Alpine Linux**
- **Void Linux**, musl edition only. The glibc edition of Void is fine.

### Windows Subsystem for Linux

WSL is not supported. It does not provide the graphics and kernel features TuxBlox depends on.

### Virtual machines

Running TuxBlox inside a VM is not supported. Graphics passthrough inside a VM is fragile enough that we cannot help debug it, and Roblox's anti-cheat is unlikely to be happy about it either.

### Containers

Running inside a container such as Distrobox works, but only if the container was created with access to your graphics card. TuxBlox detects this case and warns you on startup if the passthrough is missing. See [Common Problems](../troubleshooting/common-problems.md) for the fix.

## Checking what you have

Run these in a terminal if you want to confirm before installing:

```bash
uname -r                      # kernel version
ldd --version | head -1       # glibc version
lspci -k | grep -A3 -i vga    # graphics card and driver in use
vulkaninfo --summary | head   # Vulkan support, if vulkan-tools is installed
```
