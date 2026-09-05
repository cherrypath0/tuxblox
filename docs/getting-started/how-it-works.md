# How TuxBlox Works

Roblox is a Windows program. Linux cannot run Windows programs on its own, which is why TuxBlox exists.

TuxBlox uses a compatibility layer, a modified version of [Wine](https://www.winehq.org/) and Valve's [Proton](https://github.com/ValveSoftware/Proton), to run the real, official Roblox program on Linux. It does this without emulating a whole Windows computer and without using a virtual machine.

## How does that work?

A compatibility layer reimplements the parts of Windows that a program needs, using Linux instead. When Roblox asks Windows to do something, like open a window or read a file, TuxBlox's compatibility layer answers that request using Linux underneath. Roblox never knows the difference.

## Why does TuxBlox maintain its own compatibility layer?

A standard Wine or Proton install is not built with Roblox in mind. TuxBlox maintains its own version so it can add fixes and improvements made specifically for Roblox, including things that cannot be changed with settings alone.

## What does the TuxBlox launcher do?

Alongside the compatibility layer, the TuxBlox launcher:

- Sets up the environment Roblox needs
- Launches Roblox Studio or Roblox Player
- Installs updates automatically
- Lets you change settings, including FastFlags, from one place

You do not need to configure anything by hand.

## What TuxBlox does NOT do

- It does not emulate a whole Windows computer.
- It does not run Windows inside a virtual machine.
- It does not require WSL.
- It does not change your Linux distribution or kernel.
- It does not need a separate Windows installation.

TuxBlox runs entirely as a normal program on your system and does not need elevated privileges for everyday use.

## Why is this different from a virtual machine?

A virtual machine runs a full copy of Windows on top of Linux. That takes more resources and adds overhead.

TuxBlox skips that. It only provides the parts of Windows that Roblox actually needs, so it tends to run lighter and faster than a full virtual machine. How much faster depends on your hardware and setup.
