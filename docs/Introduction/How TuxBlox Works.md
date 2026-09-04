# How TuxBlox Works

Similar to [Wine](https://www.winehq.org/), TuxBlox allows the Windows version of Roblox to run on Linux without emulating an entire Windows computer or using a virtual machine.

TuxBlox maintains its own Wine build specifically optimized for Roblox. This allows us to make changes that cannot be achieved through configuration alone, including fixing upstream Wine bugs that affect Roblox.

## Why does Roblox not run on Linux directly?

Roblox is a Windows application (`.exe`). Linux cannot natively execute Windows applications, which is why compatibility layers such as TuxBlox and Wine exist.

Instead of running Windows itself, a compatibility layer provides the Windows APIs and runtime environment that Windows applications expect. These Windows APIs are reimplemented using Linux and Unix functionality at runtime.

## What is Wine?

[Wine](https://www.winehq.org/) (originally an acronym for "Wine Is Not an Emulator") is a compatibility layer that allows Windows applications to run on Linux and other Unix-like operating systems.

Wine reimplements Windows APIs so that Windows applications can operate using the underlying Linux system.

TuxBlox is built around a customized version of Wine specifically intended for Roblox.

## What does TuxBlox do?

TuxBlox adds a management layer around its customized Wine build. The TuxBlox launcher handles tasks such as:

* Managing the TuxBlox compatibility layer
* Configuring the environment required by Roblox
* Launching Roblox
* Handling TuxBlox updates
* Providing configuration options through the launcher's settings

This means users do not need to manually configure Wine or maintain the compatibility layer themselves.

## Why does TuxBlox maintain its own Wine build?

A standard Wine installation is not necessarily configured or patched specifically for Roblox.

TuxBlox maintains its own Wine build so that we can apply Roblox-specific fixes and improvements directly to the compatibility layer.

Some problems cannot be solved through configuration files or environment variables alone. Maintaining our own build allows us to modify the underlying compatibility layer when necessary, including fixing upstream Wine issues that affect Roblox.

## What TuxBlox and Wine do NOT do

* They do not emulate an entire Windows computer.
* They do not run Windows inside a virtual machine.
* They do not require WSL.
* They do not replace the Linux kernel.
* They do not modify your Linux distribution.
* They do not require a separate Windows installation.

TuxBlox itself operates entirely in userspace and does not require elevated privileges during normal operation.

## Why is TuxBlox different from a virtual machine?

A virtual machine runs an entire guest operating system, such as Windows, on top of the host operating system. This requires virtualized hardware and a complete Windows environment.

TuxBlox does not run an entire copy of Windows. Instead, it provides the reimplemented Windows APIs that Roblox needs directly within the Linux userspace.

This avoids the overhead of running a separate Windows operating system and virtualized hardware.

Because of this architecture, TuxBlox can provide significantly lower overhead than running Roblox inside a complete virtual machine. Actual performance can vary depending on your hardware, drivers, and configuration.
