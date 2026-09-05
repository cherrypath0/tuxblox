# Building From Source

> **This is not recommended for most people.** Building TuxBlox from source takes a while, usually one to two hours or more, and is more involved than just installing it normally. Only do this if you have a specific reason to.

Building has its own requirements, separate from the ones needed to just run TuxBlox.

## Requirements

- **Processor:** A modern x86-64 processor
- **Storage:** 50 GB or more of free space
- **Memory:** 16 GB or more of RAM
- **Packages:** `python3`, `podman`, `curl`, `gcc`, `uidmap`, `git`

Building under WSL or inside a virtual machine is not supported.

## Steps

### 1. Clone the repository

```bash
git clone https://gitlab.com/cherrypath0/tuxblox
cd tuxblox
```

### 2. Run the build script

```bash
chmod +x build.sh
./build.sh --log --nodebug
```

Do not run this with `sudo`. The script figures out your package manager and installs anything missing on its own. It will ask for your password if it needs to install packages.

It will also ask you for a version name. You can type anything you like, for example `MyBuild-1.0`.

Building again after the first time is much faster, since TuxBlox reuses what it can from the previous build.

### 3. Copy the build to your TuxBlox folder

Once the build finishes, everything is in the `build/` folder. Create the folder TuxBlox normally lives in, then copy the build there:

```bash
mkdir -p ~/.tuxblox
cp -a build/* ~/.tuxblox/
```

### 4. Run it

```bash
~/.tuxblox/TuxBloxLauncher
```

This method does not set up desktop shortcuts or URL handlers for you, so you may need to add those yourself if you want them.

Builds made this way still receive the same updates as a normal install. You are also free to modify the source code before building it.
