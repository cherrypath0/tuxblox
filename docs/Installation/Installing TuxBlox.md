# How to Install TuxBlox

TuxBlox does not currently require Flatpak or any package manager. We provide two simple ways to install TuxBlox, so choose whichever method you prefer.

## There are 2 ways to install TuxBlox:

## 1. Installer Script (Recommended)

This is the easiest way to install TuxBlox. All you need is `curl`, `bash`, and a terminal. These are already available on nearly all Linux distributions, so no additional setup should be required.

Open a terminal and run:

```bash
curl -sSLf https://tuxblox.net/install.sh | bash
```

Or, if you want to inspect the installation script before running it:

```bash
curl -sSLf https://tuxblox.net/install.sh -o install.sh
less install.sh
bash install.sh
```

The installer will automatically install the latest stable version of TuxBlox and handle the rest of the setup for you.

---

## 2. Download from our Releases page

Go to [tuxblox.net/releases](https://tuxblox.net/releases).

![TuxBlox Releases Webpage from August 27, 2026 in Firefox](https://static.tuxblox.net/images/screenshots/Docs_TuxBlox_ReleasesPage.png)

*Image taken on August 27, 2026, using the Firefox browser.*

The webpage should look similar to the screenshot above. Click the **Download** button next to `TuxBloxInstaller`.

![TuxBlox installer downloaded and shown from Firefox's downloads tab](https://static.tuxblox.net/images/screenshots/Docs_TuxBlox_InstalledFromReleasesPage.png)

Once the download is complete, click the folder icon next to `TuxBloxInstaller`. The exact appearance of this button may vary depending on your browser.

This should open your file manager. Double-click `TuxBloxInstaller` to launch it.

Some file managers, such as Dolphin, may ask you to confirm that you want to launch the executable:

![Dolphin file manager confirming launch for TuxBloxInstaller](https://static.tuxblox.net/images/screenshots/Docs_TuxBlox_DolphinConfirmation.png)

Simply click **Continue**. The TuxBlox installer should open.

If the installer refuses to open, you may need to manually make it executable. Open a terminal and navigate to the directory where you downloaded the installer. If it was downloaded to `~/Downloads`, run:

```bash
cd ~/Downloads
chmod +x TuxBloxInstaller
```

Then try launching `TuxBloxInstaller` again.

The installer should launch and look similar to this:

![TuxBlox installer from August 27, 2026](https://static.tuxblox.net/images/screenshots/Docs_TuxBlox_InstallerApplication.png)

# Running TuxBlox

After completing the installation, there are two ways to run TuxBlox.

## 1. Search for TuxBlox in your desktop environment

![TuxBlox searched on KDE Plasma's Application Launcher search bar](https://static.tuxblox.net/images/screenshots/Docs_TuxBlox_DesktopSearchBar.png)

*KDE Plasma is shown in this image.*

Open your desktop environment's application launcher and search for **TuxBlox**. Then click **TuxBlox** to launch it.

The search results may take a while to update depending on your desktop environment and configuration.

If TuxBlox does not appear, try logging out and back in, or restart your desktop shell.

---

## 2. Launch TuxBlox from the terminal

Open a terminal and run:

```bash
~/.tuxblox/TuxBloxLauncher
```

TuxBlox should now be installed and ready to use.

## Updating TuxBlox

You do not need to repeat these installation steps when updating TuxBlox. The TuxBlox launcher automatically handles updates for you.
