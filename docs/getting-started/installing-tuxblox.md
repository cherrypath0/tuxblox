# Installing TuxBlox

TuxBlox does not need Flatpak or a package manager. There are two ways to install it, pick whichever one you like.

## Option 1: Installer script (recommended)

This is the easiest way to install TuxBlox. All you need is a terminal, `curl`, and `bash`, which almost every Linux distribution already has.

Open a terminal and run:

```bash
curl -sSLf https://tuxblox.net/install.sh | bash
```

If you would rather check the script before running it:

```bash
curl -sSLf https://tuxblox.net/install.sh -o install.sh
less install.sh
bash install.sh
```

The script installs the latest stable version of TuxBlox and takes care of the rest of the setup.

## Option 2: Download from the releases page

1. Go to [tuxblox.net/releases](https://tuxblox.net/releases).
2. Download `TuxBloxInstaller`.
3. Open your downloads folder and double click `TuxBloxInstaller` to run it.

If it does not open, it may need permission to run as a program. Open a terminal in the folder you downloaded it to and run:

```bash
chmod +x TuxBloxInstaller
./TuxBloxInstaller
```

Then follow the steps in the installer window.

## Running TuxBlox

Once installed, you can start TuxBlox in either of these ways:

- **From your desktop.** Open your application launcher and search for **TuxBlox**.
- **From a terminal.** Run:
  ```bash
  ~/.tuxblox/TuxBloxLauncher
  ```

## Updating TuxBlox

You do not need to reinstall TuxBlox to update it. The launcher checks for updates and installs them automatically.
