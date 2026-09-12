# Settings

Everything on the Settings tab, in the order you see it. Changes save the moment you make them, there is no Apply button.

Settings live in `~/.tuxblox/settings.json`. You can edit that file by hand if you prefer, but the launcher rewrites it whenever you change something in the window, so do not have both open at once.

---

## Updates

### Update channel

**Default: stable**

Which release stream TuxBlox follows.

| Channel | Who it is for |
|---|---|
| `stable` | Everyone. Tested releases only. |
| `canary` | New features early, with the occasional rough edge. Still meant to be usable day to day. |
| `experimental` | Work in progress builds. Things are expected to break here. |

**If you want to be ahead of stable, use `canary`.** It is the one built for people who want new things sooner, and it is the only channel other than stable we would suggest running day to day.

`experimental` is not an "even newer canary". It is where things go to be tried, including the ones that do not work. Run it only if being broken occasionally is the point for you.

> [!NOTE]
> `experimental` used to be called `dev`. If your settings still say `dev`, TuxBlox moves you to `experimental` automatically, and `--channel dev` on the command line still works. Nothing about the channel itself changed.

Switching channels makes TuxBlox update to that channel's current release on the next check, which can mean moving backwards if stable is behind canary.

> [!WARNING]
> Bug reports from `canary` and `experimental` are welcome, but say which channel you are on. An `experimental` build may already be broken in ways nobody has looked at yet.

### Automatic updates

**Default: off**

When on, updates install themselves as soon as TuxBlox finds one.

When off, you get a notification in the corner of the launcher instead, and nothing happens until you click it.

Off is the default because an update that starts on its own while you are about to open Studio is annoying. Turn it on if you would rather never think about it.

---

## Environment

### Graphics card

**Default: Automatic**

Which graphics card Roblox renders on. This only matters on a machine with more than one, which usually means a laptop with both integrated and discrete graphics.

The dropdown lists every card TuxBlox can see, numbered, so two cards of the same model are still distinguishable. **Automatic** names whichever one your system would pick on its own.

Picking a card sets the standard Linux graphics offload variables for you, so you do not have to know which ones your driver wants. On NVIDIA that is the PRIME render offload set, and on AMD and Intel it is the Mesa device selection variables.

> [!TIP]
> If your laptop runs Studio on the wrong card and the picker does not fix it, you can override it by hand in **Environment variables** below. Anything you type there wins over the picker.

### GPU acceleration for web pages

**Default: on**

Speeds up the login screen, the Toolbox, and every other panel inside Roblox that is really a web page.

This has nothing to do with game or Studio viewport graphics. Turning it off will not affect your framerate.

Turn it off if those panels are blank, flickering, drawing garbage, or crashing. That is the most common fix for it, especially on older drivers and on llvmpipe software rendering.

Takes effect the next time Roblox starts.

### Environment variables

**Default: empty**

A space separated list of `VARIABLE=value` pairs passed to Roblox and to the compatibility layer.

```
DXVK_HUD=fps MANGOHUD=1
```

This is the escape hatch. Anything you put here is applied last, so it overrides variables TuxBlox sets for you, including the graphics card picker.

See [Environment Variables](../advanced/environment-variables.md) for what is worth setting.

---

## Controller

### Enable Haptics

**Default: on**

Lets Roblox vibrate your controller. This is experimental.

Roblox drives vibration through XInput, which is Microsoft's controller interface. An Xbox pad speaks it natively. A PlayStation pad does not, so the compatibility layer has to route it a different way for the motors to be reachable at all, and that route costs one button on the pad.

If a button on your controller is not responding, turn this off. Otherwise leave it on.

Takes effect the next time Roblox starts.

---

## Privacy

### Send crash reports

**Default: on**

When Roblox exits badly, TuxBlox sends a report containing:

- The exit code
- Your Roblox and TuxBlox versions
- Basic system information, such as distribution, kernel and graphics driver
- A copy of the session log

Nothing else is collected and nothing is sent while things are working normally. The full policy is at [tuxblox.net/privacy](https://tuxblox.net/privacy).

Turn it off if you would rather send nothing. It costs us useful information but it is your call, and TuxBlox works exactly the same either way.

---

## Danger zone

Three buttons that do real damage. The bottom two need two clicks: the first arms them, the second does it.

### Terminate Roblox

Stops every Roblox process that is currently running. Player, Studio, the crash handler, all of it.

Use this when Roblox has stopped responding and will not close normally, or when the launcher still thinks Roblox is running after it has gone.

Unlike the other two, this is safe. The worst it can do is lose unsaved work in Studio.

### Wipe prefix

Deletes the virtual drive and everything in it.

That includes every installed Roblox version, anything Roblox saved locally, and any Windows settings inside the drive. Your TuxBlox settings and FastFlags are kept, since those live outside the drive.

The next launch rebuilds the drive and reinstalls Roblox from scratch, which takes about as long as your very first launch did.

This is the biggest hammer for "Roblox is behaving strangely and I do not know why". Try it before reinstalling TuxBlox.

> [!CAUTION]
> Anything you put inside the virtual drive by hand goes with it. Copy it out first.

### Uninstall TuxBlox

Removes TuxBlox completely: the virtual drive, the desktop shortcuts, the file associations, and TuxBlox itself.

The equivalent from a terminal is `~/.tuxblox/TuxBloxInstaller --uninstall`.
