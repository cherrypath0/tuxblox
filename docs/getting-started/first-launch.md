# Your First Launch

The first time you start Roblox through TuxBlox takes noticeably longer than every time after it. This page explains why, so you know the difference between "still working" and "stuck".

## Opening the launcher

Search for **TuxBlox** in your applications menu, or run `~/.tuxblox/TuxBloxLauncher`.

You land on the **Home** tab, with a card for Roblox Player and one for Roblox Studio. Along the bottom is a status strip showing your TuxBlox version, your update channel, and whether an update is waiting.

Only one launcher window can be open at a time. Starting a second one tells you the first is already running.

## Pressing Launch

The first time, the Studio card's button reads **Install & Launch** instead of **Launch Studio**. That is TuxBlox telling you Roblox is not in the virtual drive yet.

Press it and these things happen in order:

1. **The virtual drive is prepared.** This is the Windows-shaped folder Roblox lives in. TuxBlox builds it from scratch the first time.
2. **Roblox's own installer is downloaded** from Roblox's servers and run inside that drive. TuxBlox does not host or repackage Roblox, it fetches the real thing.
3. **Roblox installs itself**, exactly as it would on Windows.
4. **Studio starts.**

Expect this to take a few minutes on a normal connection. Most of it is downloading.

> [!NOTE]
> The launcher window closes once Roblox starts. That is intentional, not a crash. A small helper process stays behind to watch the session, and it is what shows you a message if Roblox exits badly.

## Signing in

Studio's login screen is a web page rendered inside the application. TuxBlox ships its own browser component for this, so it works without you installing anything extra.

Log in the same way you would on Windows. Your session is remembered afterwards.

> [!TIP]
> If the login screen is blank, slow, or flickers, turn off **GPU acceleration for web pages** in [Settings](../using-tuxblox/settings.md#gpu-acceleration-for-web-pages) and start Roblox again. That setting is the usual culprit on older graphics drivers.

## Every launch after the first

Later launches skip all of the setup and go straight to starting Roblox. On most machines that is a few seconds.

Roblox updates itself from then on, the same way it does on Windows. You can also pin a specific Roblox build from the [Versions](../using-tuxblox/versions.md) tab if you would rather decide yourself.

## What about Roblox Player?

Roblox Player does not start yet. Roblox's anti-cheat blocks it under every Wine based compatibility layer, TuxBlox included. Getting it working is the project's largest ongoing piece of work.

The Player card is in the launcher because the work is active, not because it is finished. Studio is unaffected and works normally. See [Known Issues](../troubleshooting/known-issues.md) for the current status.

## If something went wrong

- Nothing happened at all, or a window flashed and vanished: [Common Problems](../troubleshooting/common-problems.md).
- It started but looks wrong: also [Common Problems](../troubleshooting/common-problems.md), the graphics section.
- You want to send a log to someone: [Logs and Diagnostics](../troubleshooting/logs.md).
