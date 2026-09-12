# Common Problems

Things that go wrong often enough to have a known answer. If your problem is here, try the fix. If it is not, check [Known Issues](known-issues.md), then [Reporting Bugs](reporting-bugs.md).

---

## Installing

### The installer will not open when I double click it

Your system has not marked it as a program yet. In a terminal, in the folder you downloaded it to:

```bash
chmod +x TuxBloxInstaller
./TuxBloxInstaller
```

### The installer opens and then closes immediately

Run it from a terminal so you can see what it says:

```bash
./TuxBloxInstaller
```

The most common cause is not enough free disk space. TuxBlox needs about 6 GB, and Roblox needs more on top of that.

### Nothing appears in my applications menu after installing

Start the launcher once from a terminal and let it sit for ten seconds:

```bash
~/.tuxblox/TuxBloxLauncher
```

It rewrites its desktop entries every time it starts and refreshes your desktop's caches. If entries still do not appear, log out and back in.

---

## Starting Roblox

### Nothing happens when I press Launch

Start the launcher from a terminal and press Launch again. Errors that a window has no room for go to the terminal.

```bash
~/.tuxblox/TuxBloxLauncher
```

### The launcher window disappears when I press Launch

That is supposed to happen. Once Roblox is running the launcher hands off to a small background watcher and closes. See [The Launcher](../using-tuxblox/the-launcher.md#the-launcher-closes-when-roblox-starts).

### Roblox Player will not start

Player does not work yet under any Wine based compatibility layer. This is not something you have misconfigured. See [Known Issues](known-issues.md).

### Studio starts and then closes on its own

Check the log for the session in `~/.tuxblox/logs`. See [Logs and Diagnostics](logs.md) for what to look for.

If you have FastFlags set, clear them all and try again. A flag that expects a number but got text is a common cause and gives no useful error.

### "An instance of the TuxBlox Launcher is already running"

Only one launcher window is allowed at a time. If no window is visible, one is still running in the background:

```bash
pkill -f TuxBloxLauncher
```

Then start it again.

---

## Graphics

### The login screen is blank, flickering, or full of garbage

Turn off **GPU acceleration for web pages** in Settings and start Roblox again. That is the fix for this the large majority of the time, especially on older drivers and on software rendering.

### Studio's viewport is black or blank

Usually a driver issue rather than a TuxBlox one. In order of how often they work:

1. Update your graphics drivers, and Mesa if you are on AMD or Intel.
2. Confirm Vulkan works at all: `vulkaninfo --summary`, from the `vulkan-tools` package.
3. If your machine has two graphics cards, set the right one under **Graphics card** in Settings.

### Roblox runs on the wrong graphics card

Pick the right one under **Graphics card** in Settings. The dropdown lists every card TuxBlox can see.

If that does not take effect, set the variables by hand in **Environment variables**, which override the picker. See [Environment Variables](../advanced/environment-variables.md#picking-a-card).

### Performance is worse than I expected

Things worth checking, roughly in order of impact:

- **Your graphics drivers.** This is the single biggest factor and it is not close.
- **Which card you are on.** A laptop that quietly picked integrated graphics loses most of its performance right there.
- **Whether you have logging on.** `TUXBLOX_LOG` costs real frame time. Leave it off unless you are debugging.
- **FastFlags you copied from a list.** Clear them and measure again.

TuxBlox does not use DXVK for actual gameplay, because Roblox renders through Vulkan directly. DXVK settings and overlays will not change your in-game framerate.

---

## Controllers and audio

### A button on my controller stopped working

Turn off **Enable Haptics** in Settings and restart Roblox.

Vibration on a non-Xbox controller has to be routed a way that costs one button. If you would rather have the button than the rumble, that toggle is the trade.

### My controller is not detected at all

Check the system sees it first:

```bash
ls /dev/input/js* /dev/input/event*
```

If your desktop does not see the controller either, the problem is below TuxBlox, in your kernel or udev rules.

### My headset is treated as speakers

Some USB headsets tell the system they are speakers, and there is no way for TuxBlox to tell the difference. It is not detectable in software, so it cannot be fixed automatically. Ask in [Discord](https://tuxblox.net/discord) for the registry override.

---

## Updating

### TuxBlox says an update is available but nothing happens

**Automatic updates** is off, which is the default. Click the notification in the corner of the launcher to apply it.

### An update failed halfway through

Run the installer again. It upgrades an existing install in place and will finish what was left.

```bash
~/.tuxblox/TuxBloxInstaller
```

### I want to go back to the previous version

TuxBlox has no rollback button. Install the version you want from [tuxblox.net/releases](https://tuxblox.net/releases), and switch your update channel to `stable` so it does not immediately pull you forward again.

---

## Containers

### Distrobox GPU passthrough warning

If TuxBlox is running inside a Distrobox container without access to your graphics card, it tells you so on startup, because Roblox will almost certainly fail to render.

The container has to be recreated with GPU access, that cannot be added afterwards:

```bash
# NVIDIA
distrobox create --nvidia --name tuxblox --image ubuntu:24.04

# AMD or Intel
distrobox create --name tuxblox --image ubuntu:24.04 \
  --additional-flags "--device /dev/dri"
```

---

## When nothing here helps

In escalating order:

1. **Clear your FastFlags.** They are the most common cause of unexplainable behaviour.
2. **Wipe the prefix.** Settings, Danger zone, Wipe prefix. This resets Roblox completely and keeps your settings.
3. **Reinstall TuxBlox.** Uninstall from Settings, then install again.
4. **Ask.** [Discord](https://tuxblox.net/discord) with your log file attached, or open an issue. See [Reporting Bugs](reporting-bugs.md).
