# Installing TuxBlox

TuxBlox does not need Flatpak, Snap, or your distribution's package manager. It installs into a single folder in your home directory and never asks for root.

There are two ways to install it. Both end up in the same place.

## Option 1: The install script

This is the easiest way. You need a terminal, `curl` and `bash`, which practically every Linux distribution already has.

```bash
curl -sSLf https://tuxblox.net/install.sh | bash
```

That downloads the installer, runs it, and starts TuxBlox when it finishes.

<details>
<summary>Prefer to read the script before running it?</summary>

Reasonable. Piping a script from the internet into your shell is a habit worth being careful about.

```bash
curl -sSLf https://tuxblox.net/install.sh -o install.sh
less install.sh
bash install.sh
```

</details>

## Option 2: The installer from the releases page

1. Go to [tuxblox.net/releases](https://tuxblox.net/releases).
2. Download **TuxBloxInstaller**.
3. Double click it in your file manager.

If double clicking does nothing, your system has probably not marked it as a program yet. Open a terminal where you downloaded it and run:

```bash
chmod +x TuxBloxInstaller
./TuxBloxInstaller
```

A window opens and walks you through the rest.

> [!TIP]
> On a machine with no desktop, such as a server you are testing on over SSH, run `./TuxBloxInstaller --headless` instead. It reports progress in the terminal and never tries to open a window. See [Command Line Reference](../advanced/command-line.md) for the other flags.

## What the installer actually does

Nothing surprising, and nothing outside your home folder:

- Creates `~/.tuxblox` and downloads TuxBlox into it.
- Writes desktop entries so TuxBlox appears in your applications menu.
- Registers `roblox:` links and `.rbxl` files so they open in TuxBlox.
- Starts the launcher.

It never asks for your password, never touches system directories, and never installs a background service.

## Starting TuxBlox

Once installed, you can start it in either of these ways:

- **From your desktop.** Open your applications menu and search for **TuxBlox**.
- **From a terminal.** Run `~/.tuxblox/TuxBloxLauncher`.

Next: [Your First Launch](first-launch.md).

## Updating

You do not reinstall to update. The launcher checks for updates when it starts and, depending on your settings, either installs them or shows you a notification first. See [Settings](../using-tuxblox/settings.md#updates) to change which one it does.

## Uninstalling

Open the launcher, go to **Settings**, scroll to **Danger zone**, and press **Uninstall TuxBlox** twice.

From a terminal, the equivalent is:

```bash
~/.tuxblox/TuxBloxInstaller --uninstall
```

Either one removes `~/.tuxblox` entirely, including your virtual drive and everything Roblox installed into it, plus the desktop entries and file associations.
