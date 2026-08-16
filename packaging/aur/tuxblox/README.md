# AUR packaging for TuxBlox

This directory is the source of truth for the [AUR](https://aur.archlinux.org)
package `tuxblox`. It ships the official prebuilt `TuxBloxInstaller` release
binary; on first run that installer performs the normal per-user install into
`~/.tuxblox` (Proton build, launcher, its own persisted copy, Roblox installer
cache), registers the launcher's menu entry and `roblox://` URL handlers, and
execs the launcher — identical to downloading the installer from tuxblox.net.

What the package itself installs:

| Path | What |
|---|---|
| `/usr/bin/tuxblox-installer` | The release `TuxBloxInstaller` binary, byte-identical to the website download |
| `/usr/bin/tuxblox` | Wrapper: execs `~/.tuxblox/TuxBloxLauncher` if present, else the installer |
| `/usr/share/applications/tuxblox.desktop` | "TuxBlox Installer" menu entry (bootstrap; the launcher registers its own entry after first run) |
| `/usr/share/pixmaps/tuxblox.png` | Icon |

## Why a binary repackage instead of building from source

- The installer's source lives in this monorepo, whose git tree is several
  gigabytes (it vendors a full Wine fork) — unreasonable to clone for a
  1.5 MB bootstrap binary, and GitHub offers no subdirectory tarballs.
- The actual product (the patched Proton build) cannot be built by makepkg at
  all: it needs the pinned old-glibc podman container, takes hours, and is
  downloaded at runtime by the installer regardless. Source-building just the
  bootstrap would buy nothing.
- The versioned artifact URLs (`/v1/stable/<version>/installer`) are
  immutable, and the sha256 pinned in the PKGBUILD is the one published in the
  release manifest — the same value the installer itself verifies downloads
  against.

AUR convention would call a binary repackage `tuxblox-bin`; since no source
package is feasible and this is maintained by upstream, it uses the plain
name `tuxblox` (same pattern as other upstream-distributed binaries on the
AUR). If a source-built variant ever becomes viable, that one can take a
`-git`/source name alongside.

## One-time AUR setup

1. Add your SSH key to your AUR account (aur.archlinux.org → My Account).
2. Clone the (initially empty) AUR repo somewhere outside this repo:
   ```
   git clone ssh://aur@aur.archlinux.org/tuxblox.git ~/aur-tuxblox
   ```
3. Copy the package files in and push:
   ```
   cp PKGBUILD .SRCINFO tuxblox.sh tuxblox.desktop tuxblox.png tuxblox.install .gitignore ~/aur-tuxblox/
   cd ~/aur-tuxblox && git add -A && git commit -m "Initial import: tuxblox 0.2.0" && git push
   ```
   (`update.sh` and this README are maintainer tooling — they stay in this
   repo and are not part of the AUR upload, though including them would be
   harmless.)

## Releasing a new version

After `package.sh` has published a release to setup.tuxblox.net:

```
./update.sh
```

This reads `/v2/latest.json` for the stable version and the per-version
manifest for the installer's sha256, rewrites `pkgver`/`pkgrel`/the first
`sha256sums` entry, and regenerates `.SRCINFO`. Then:

1. Test-build locally: `makepkg -f` (and optionally install with `-i`).
2. Copy `PKGBUILD` + `.SRCINFO` to the AUR checkout, commit, push.

`.SRCINFO` must be regenerated on every AUR push (`makepkg --printsrcinfo >
.SRCINFO`) — `update.sh` already does this. Note the AUR displays whatever
was pushed; there is no separate release step.

If you change `tuxblox.sh`, `tuxblox.desktop`, or `tuxblox.png`, re-run
`updpkgsums` (pacman-contrib) or update their `sha256sums` entries by hand,
bump `pkgrel`, regenerate `.SRCINFO`, and push those files too.

## File notes

- `.SRCINFO` is generated output (`makepkg --printsrcinfo`) and `tuxblox.png`
  is a binary asset fetched from `static.tuxblox.net` — neither carries the
  GPL header comment that TuxBlox source files otherwise start with.
- The per-user install is never touched by pacman: `pacman -R tuxblox` leaves
  `~/.tuxblox` (and any Roblox login in its Wine prefix) intact, matching the
  behavior described in `tuxblox.install`'s post_remove message.
