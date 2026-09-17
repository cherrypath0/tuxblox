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

Switching channels makes TuxBlox move to that channel's current release on the next check. That can mean going backwards, if the channel you picked is on an older version than the one you have — you asked for that channel, so TuxBlox gives you what is on it.

The channel shown at the bottom of the Home tab is the one the version you are running came from, so it changes when the update lands rather than the moment you pick a new channel.

> [!WARNING]
> Bug reports from `canary` and `experimental` are welcome, but say which channel you are on. An `experimental` build may already be broken in ways nobody has looked at yet.

### Automatic updates

**Default: off**

When on, updates install themselves as soon as TuxBlox finds one.

When off, you get a notification in the corner of the launcher instead, and nothing happens until you click it.

Off is the default because an update that starts on its own while you are about to open Studio is annoying. Turn it on if you would rather never think about it.

One case ignores this setting. TuxBlox is several programs that ship together and are meant to be the same version, and an update interrupted partway through can leave them mismatched. TuxBlox checks for that at startup and repairs it straight away, because a mismatched install is broken rather than merely out of date. It is not something you can end up in by choosing to postpone an update.

This setting is about TuxBlox itself. Roblox has its own setting below.

### Auto-Update Roblox

**Default: on**

Before each launch, TuxBlox checks whether Roblox has put out a newer build and installs it if so. The check itself is silent; a window appears only when there is actually something to download.

When off, you keep whichever build you have until you install another one yourself from the Versions tab.

On is the default because this is what happens on Windows too, and a Roblox that is behind can refuse to join an experience. If the check cannot finish, TuxBlox starts the Roblox you already have rather than stopping, and says so in a desktop notification.

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

### Virtual Desktop Mode

**Default: off**

Runs Roblox inside a single window of its own, instead of letting it place windows on your desktop. This is experimental.

Normally every Roblox window is a window on your desktop, moved and resized by your desktop like any other. With this on, Roblox gets a desktop of its own inside one window, and everything it opens stays inside that window. It is sized to your screen.

This is worth trying when windows open in the wrong place, go missing, or will not give your mouse back. It is not better than the normal mode otherwise, and your desktop can no longer arrange Roblox's windows for you.

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

## Troubleshooting

### Detailed logging

**Default: off**

Records far more about what Roblox and the compatibility layer are doing, in the same session log TuxBlox already saves for every launch. Reporting a problem still means sending one file.

It makes Roblox slightly slower and log files much larger, which is why it is a setting rather than something that is always on. Leave it off unless you are reproducing a problem to report.

Takes effect the next time Roblox starts.

---

## Privacy

### Verify Roblox Integrity

**Default: on**

Before launching, TuxBlox checks that Roblox's files carry a valid signature from Roblox and have not been altered since. If that check fails, TuxBlox refuses to launch and tells you to reinstall Roblox.

Leave it on. The check costs a moment at startup and is the one thing standing between you and running a Roblox that somebody else has modified.

If it refuses to launch something you believe is fine, make sure TuxBlox itself is up to date first, then reinstall Roblox. Turning this off to get past the warning means launching files that failed verification, and you should be sure that is what you want.

### Send crash reports

**Default: off**

When Roblox exits badly, TuxBlox sends a report containing:

- The exit code
- Your Roblox and TuxBlox versions
- Basic system information, such as distribution, kernel and graphics driver
- A copy of the session log

Nothing else is collected and nothing is sent while things are working normally. The full policy is at [tuxblox.net/privacy](https://tuxblox.net/privacy).

Turn it on if you would like to help. It gives us information we genuinely cannot get any other way, but it is your call, and TuxBlox works exactly the same either way.

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
