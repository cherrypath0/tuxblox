# Repository Structure

A quick tour of what lives where in the TuxBlox repository.

## Two halves, two licenses

TuxBlox's repository is split into two parts with different licenses:

- **Everything outside `compat/`** is TuxBlox's own launcher and installer code, licensed under **GPLv3**.
- **`compat/`** is TuxBlox's compatibility layer, based on Wine and Proton, licensed under **LGPLv2.1**.

These are separate programs that talk to each other while running, not one program built from both halves. Keeping that separation is important, it is what lets the project use two different licenses.

## Top level folders

- **`installer/`**, the source code for `TuxBloxInstaller`.
- **`launcher/`**, the source code for `TuxBloxLauncher`, which sets up and runs Roblox.
- **`compat/`**, TuxBlox's compatibility layer. Made up of many smaller pieces (submodules), plus a Wine fork maintained directly inside this repository. `compat/tuxblox/` holds TuxBlox's own code for the layer, and `compat/webkitgtk/bundle/` holds TuxBlox's own code for the bundled web browser component used for things like the Roblox login screen and Toolbox — these two are the only parts of it that are GPLv3.
- **`docs/`**, this documentation.
- **`include/`**, extra files that get copied into the finished build.
- **`third_party_licenses/`**, license text for other software bundled with TuxBlox.

## Top level files

- **`build.sh`**, builds the installer, launcher, and compatibility layer, in that order. See [Building From Source](building-from-source.md).
- **`Containerfile`**, describes the container TuxBlox is built inside, to keep builds consistent across different machines.
- **`CONTRIBUTING.md`**, how to contribute to TuxBlox.
- **`LICENSE`**, the GPLv3 license covering TuxBlox's own code.
