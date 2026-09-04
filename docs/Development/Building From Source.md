> **This option is not recommended.** 
> This step is very complex, and TuxBlox's code base is very big. Building from the source code will take about 1-2 hours or more, potentially longer on older CPUs. Only do this if you have a powerful computer, have the patience to wait for hours, and have a specific reason why.

# Building From Source

Installing by building from source code has its own requirements, separate from the runtime requirements.

***

### Hardware Requirements:
* **Processor:** Modern processor with x86-64 architecture
* **Storage Space:** 50 GB or more
* **Memory:** 16 GB or more
### Software Requirements:
* **Operating System:** Ubuntu 20.04 LTS, Debian 11 Bullseye, Fedora 32, Arch Linux, or newer, with glibc 2.31+ support
* **Packages:** python3, podman, curl, gcc, uidmap, git (This may differ depending on your package manager)

Building under WSL (Windows Subsystem for Linux) or inside virtual machines is not supported.

***

## How to Build TuxBlox from Source

### 1. Clone into the GitLab repository
Open a terminal and run this command:
```bash
git clone https://gitlab.com/cherrypath0/tuxblox
cd tuxblox
```

***

### 2. Run build.sh
Run the `build.sh` file in the repository's root, this file will ensure that you have all the required packages installed.

Make the file executable first:
```bash
chmod +x build.sh
```

After that, run the file, with the recommended options:
```bash
./build.sh --log --nodebug
```
**Do not run this with sudo.** The script will automatically detect your package manager and install the required dependencies. It may need sudo privileges to install packages, so it will prompt you for your password if your user account has sudo privileges.

Building after a successful build will be significantly faster than before, because TuxBlox uses ccache by default, speeding up the building process.

The file will prompt for a version, you can type any version, for example: `MyLocalTuxBlox-1.0`, `TuxBloxFromSource`, etc.
Assuming there were no errors, continue to the next step.

***

### 3. Go into the build directory
After success, it will produce the installer, the launcher, and the compatibility layer in the `build/` directory.

Delete junk files to save storage, this is safe to delete because it only contains compiler artifacts and is not required for TuxBlox:
```bash
cd build
rm -rf .artifacts
```

***

### 4. Copy everything to the TuxBlox directory
The TuxBlox directory is at `~/.tuxblox`, the launcher will always look there.

Create the directory first:
```bash
mkdir -p ~/.tuxblox
```

After that, copy everything into `~/.tuxblox`, note that this may take a long time depending on your drive:

```bash
cp -a ./* ~/.tuxblox/
```

***

You have successfully built and installed TuxBlox! Note that this method does not install URL handlers or desktop shortcuts, so you might need to do more setup, start the launcher with:
```bash
~/.tuxblox/TuxBloxLauncher
```

Feel free to delete the repository folder to free up storage.

Users who have built TuxBlox from source still receive the same updates as users who installed TuxBlox from the website. Feel free to locally modify the source code before building.