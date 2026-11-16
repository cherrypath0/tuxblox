This is a very big stable release for TuxBlox with some of the biggest changes TuxBlox has ever seen.

# Flags system
We have introduced a new system to TuxBlox, and it's called flags, it is a set of internal TuxBlox settings that are far more advanced than the normal settings, it allows you to internally change some things without having to rebuild or get a different version of TuxBlox.

Our servers can also change some of your flags, however your on-disk flags override the server's flags, we only change flags on our side if we want to lock a feature until a certain point, or if we want to fix issues without releasing a new version of TuxBlox.

More documentation on TuxBlox's flags feature can be found here: <https://tuxblox.net/docs/advanced/tuxblox-flags>

We're introducing 3 new installation options:

# 1. GitHub releases
**TuxBlox is now available from GitHub releases.** Every release is published there with all its files and changelogs. Installing works exactly the same either way, so use whichever you prefer.

<https://github.com/cherrypath0/tuxblox/releases>

# 2. Flatpak Installation
**TuxBlox is now available as a Flatpak!** You can either install the Flatpak through Flathub, or through our servers.

Some features in Flatpak had to deliberately be disabled because of its sandbox, and since Flatpak handles updates itself, TuxBlox's auto-updater is disabled on Flatpak builds.

## From Flathub (Recommended)
Run:
```bash
flatpak install net.tuxblox.TuxBlox
```

## From our servers
First, obtain the Flatpak for this release, either through GitHub releases, or the website.
After you obtain the Flatpak file (`.flatpak` extension), run:
```bash
flatpak install ./net.tuxblox.TuxBlox.flatpak
```

# 3. AppImage installers
**We now distribute the TuxBlox installer as an AppImage!** Obtaining it is very easy, go to either the website or the GitHub releases, then obtain the `TuxBloxInstaller.AppImage` file.
Make sure that the AppImage has permissions to run on your system:
```bash
cd ~/Downloads # or whatever folder you have downloaded it to
chmod +x ./TuxBloxInstaller.AppImage
```

After that, either double click the AppImage through your file manager, or run this in your terminal:
```
./TuxBloxInstaller.AppImage
```
***

This release didn't only affect how you install TuxBlox, but also **how you build TuxBlox**.

# An easier way to build TuxBlox
Building TuxBlox used to be complicated: a `build.sh` script, assets fetched online on every build, and a full rebuild every time. All of that is gone, replaced by simple `make` commands.

- **Offline builds.** Sources are downloaded once, checked against recorded checksums, and reused. The build itself runs with no network access.
- **Faster rebuilds.** Only what changed gets rebuilt. Building the launcher a second time takes about two seconds.
- **Reproducible.** Building the same source twice now produces the same download.

Full documentation: https://tuxblox.net/docs/development/building-from-source

(This step is not required for anyone who just wants to download and run TuxBlox)