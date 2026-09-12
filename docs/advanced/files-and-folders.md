# Files and Folders

Everything TuxBlox installs lives in one folder: `~/.tuxblox`. The only exceptions are the desktop entry and icon files, which have to go in standard XDG locations for your desktop to find them.

Nothing is installed system wide, and nothing needs root.

## The TuxBlox folder

```
~/.tuxblox/
├── TuxBloxLauncher        the launcher you use
├── TuxBloxInstaller       kept around for updates and uninstalling
├── mcp.sh                 the Studio MCP helper
├── settings.json          your settings and FastFlags
├── versions.json          which Roblox builds are installed
├── launcher.lock          stops two launchers running at once
├── COPYRIGHT.txt          notices for everything bundled
├── LICENSE                the GPLv3 text
├── compat/                the compatibility layer
├── libtuxblox/            libraries the launcher's interface needs
├── logs/                  session logs
├── runtime/               the virtual drive
├── RobloxPlayer/          Roblox's own downloaded installer
└── RobloxStudio/          the same, for Studio
```

### settings.json

Everything from the Settings tab, plus your FastFlags.

```json
{
  "channel": "stable",
  "auto_update": false,
  "env_vars": "",
  "gpu": "",
  "haptics": true,
  "webview_gpu": true,
  "send_crash_reports": true,
  "fast_flags": {
    "player": [],
    "studio": []
  }
}
```

You can edit it by hand. TuxBlox rewrites the file whenever you change something in the launcher, so do not have both going at once.

If the file is missing or damaged, TuxBlox falls back to defaults rather than refusing to start. A broken settings file costs you your settings, never your install.

### versions.json

Bookkeeping for the Versions tab: which Roblox builds are installed, which one is active, and when each arrived.

TuxBlox does not trust this file on its own. It checks the virtual drive and corrects the file against what is actually there, so deleting it is harmless.

### compat/

The compatibility layer.

| | |
|---|---|
| `main` | The entry point. Run `main --version` to see which build is installed. |
| `files/` | Wine, DXVK, the bundled browser component, fonts, libraries |
| `LICENSE`, `third_party_licenses/` | LGPLv2.1 and everything else bundled |

This folder is replaced wholesale by updates. Anything you put in it goes away.

### libtuxblox/

Libraries the launcher's interface needs, so the launcher looks and behaves the same regardless of what your distribution ships.

It has to stay next to `TuxBloxLauncher`. Moving one without the other breaks the launcher.

### logs/

One file per Roblox session, named after the app, the start time in UTC, and the process id:

```
Studio-20260912T143107Z-8814.log
```

Roblox's own logs from the same session are appended to the end of the file, so one log has both halves of the story. See [Logs and Diagnostics](../troubleshooting/logs.md).

Old logs are never deleted automatically. Clear them out yourself if the folder gets large.

---

## The virtual drive

`~/.tuxblox/runtime/` is the virtual drive, the Windows shaped world Roblox lives in.

```
runtime/
├── pfx/
│   ├── drive_c/        the C: drive
│   ├── dosdevices/     drive letter mappings
│   ├── system.reg      the registry
│   ├── user.reg
│   └── userdef.reg
├── version             which TuxBlox built this drive
├── config_info         what it was built with
└── tracked_files       which files TuxBlox owns, as opposed to Roblox
```

Roblox itself installs into `pfx/drive_c/`, in a per user folder, with one directory per installed build.

`tracked_files` is how an update can replace TuxBlox's own files without touching anything Roblox or you put in the drive.

> [!WARNING]
> Do not share your virtual drive, and do not put it in a bug report. It can contain your Roblox session cookies, which is the same as handing over your account. Send the log file instead.

### There is no Z: drive

On most Wine setups, `Z:` maps your entire Linux filesystem into the Windows world. TuxBlox removes it, because a `Z:` drive is one of the most reliable ways to detect that a program is running under Wine, and Roblox lives entirely under `C:` anyway.

The practical effect: a Windows program run inside the drive cannot see your home folder. When you double click a `.rbxl` file, TuxBlox bridges that one file in rather than exposing everything.

### Resetting it

**Wipe prefix** in Settings deletes the whole thing. The next launch rebuilds it and reinstalls Roblox, which takes about as long as your first launch did.

Your settings and FastFlags live outside the drive and survive.

---

## Outside the TuxBlox folder

| Path | What is there |
|---|---|
| `~/.local/share/applications/` | Desktop entries for TuxBlox, Roblox Studio, Roblox Player, and the link handlers |
| `~/.local/share/icons/hicolor/` | Icons for those entries |
| `~/.local/share/mime/packages/` | The `.rbxl` and `.rbxlx` file type definitions |

That is the complete list. Uninstalling removes all of it.

---

## Backing things up

| Worth keeping | Path |
|---|---|
| Your settings and FastFlags | `~/.tuxblox/settings.json` |
| Anything you saved inside the drive | `~/.tuxblox/runtime/pfx/drive_c/users/` |

Everything else can be downloaded again. There is no point backing up `compat/` or an installed Roblox build.
