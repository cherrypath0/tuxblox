Another huge patch for TuxBlox, and this time it's not just the binary that changed, but the repository too!

# Download from GitHub releases
You asked for it: TuxBlox is now also available from GitHub releases. Every release is published there with all its files attached and this changelog as the release notes.

Installing works exactly the same either way, so use whichever you prefer.

https://github.com/cherrypath0/tuxblox/releases

# An easier way to build TuxBlox
This one is for anyone who builds from source, with or without changes of their own.

Building TuxBlox used to be complicated: a `build.sh` script, assets fetched online on every build, and a full rebuild every time. All of that is gone, replaced by simple `make` commands.

- **Offline builds.** Sources are downloaded once, checked against recorded checksums, and reused. The build itself runs with no network access.
- **Faster rebuilds.** Only what changed gets rebuilt. Building the launcher a second time takes about two seconds.
- **Reproducible.** Building the same source twice now produces the same download.

Full documentation: https://tuxblox.net/docs/development/building-from-source