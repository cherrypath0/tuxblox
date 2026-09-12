# How TuxBlox Works

Roblox is a Windows program. Linux cannot run Windows programs on its own. That is the whole problem TuxBlox exists to solve.

You do not need to understand any of this to use TuxBlox. It is here because plenty of people want to know what is running on their machine.

## The short version

TuxBlox uses a **compatibility layer**, which is a program that answers Windows requests using Linux underneath. When Roblox asks Windows to open a window, read a file, or draw a triangle, TuxBlox catches that request and does the equivalent Linux thing. Roblox never finds out.

There is no Windows installed anywhere. There is no virtual machine. Roblox's code runs directly on your processor, at full speed.

TuxBlox's layer is built on [Wine](https://www.winehq.org/) and Valve's [Proton](https://github.com/ValveSoftware/Proton), both open source, both heavily modified here.

## Translation, not emulation

These two words get mixed up a lot, so it is worth being precise.

| | Emulator | Compatibility layer |
|---|---|---|
| What it does | Pretends to be different hardware | Answers a different operating system's requests |
| Speed cost | Large, often several times slower | Small, usually a few percent |
| Example | Running a Game Boy game on a PC | TuxBlox, Wine, Proton |

An emulator has to fake a processor. TuxBlox does not, because your processor already speaks the language Roblox was compiled into. Only the operating system underneath is different, and that is the part TuxBlox replaces.

## The virtual drive

Windows programs expect a `C:` drive, a Program Files folder, a registry, and a user profile. TuxBlox gives Roblox all of those inside one ordinary folder on your disk, at `~/.tuxblox/runtime`.

We call it **the virtual drive**. Wine calls it a prefix, if you have read about this elsewhere.

Everything Roblox installs, saves, or writes down goes in there. Nothing escapes into the rest of your system. Deleting that one folder resets Roblox completely and leaves everything else untouched.

## Why TuxBlox maintains its own layer

A stock Wine or Proton install is built to run thousands of different programs reasonably well. TuxBlox only cares about two, so it can make choices a general purpose layer cannot:

- Fixes aimed at Roblox specifically, including ones that change how the layer behaves at a level no setting can reach.
- Parts of Windows that Roblox uses and Wine had not filled in yet.
- Removal of the pieces Roblox never touches, which makes the download smaller and startup faster.
- Behaviour matched closely to real Windows in the places Roblox actually checks.

This is also why the layer is offered as a separate download but comes with no promise about other software. It was not built for other software.

## What the launcher does

The compatibility layer is only half of TuxBlox. The launcher is the part you see, and it handles:

- Building and updating the virtual drive
- Downloading and installing Roblox itself
- Starting Studio or Player and watching the session
- Keeping TuxBlox up to date
- Settings, FastFlags, and Roblox version management
- Desktop shortcuts, `roblox:` links and `.rbxl` file associations

The point of all that is so you never have to configure anything by hand. If you want to anyway, the [Advanced](../advanced/environment-variables.md) section is there.

## What TuxBlox does not do

- It does not emulate a computer.
- It does not run Windows in a virtual machine.
- It does not need WSL.
- It does not change your distribution, your kernel, or anything outside your home folder.
- It does not need root. Not during install, not during use.
- It does not modify Roblox, and it does not help anybody cheat.

That last one is deliberate policy, not an accident of design. TuxBlox hides the fact that Roblox is running on Linux, because Roblox refuses to start otherwise. It does nothing to hide what a player is doing inside the game.
