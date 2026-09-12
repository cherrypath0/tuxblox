# Logs and Diagnostics

When something breaks, the log is the first thing anyone will ask you for.

## Where logs live

`~/.tuxblox/logs/`

One file per session. The name is not random, it tells you which app ran and exactly when:

```
Studio-20260912T143107Z-8814.log
│      │                └─ the process id that ran the session
│      └─ when it started, in UTC
└─ Studio or Player
```

The time is UTC rather than your local clock, so a log filename means the same thing to everyone reading it. `20260912T143107Z` is 12 September 2026, 14:31:07 UTC.

The process id on the end is only there to keep two sessions started in the same second apart. You can ignore it.

The newest file is the run you just did:

```bash
ls -t ~/.tuxblox/logs | head -1
```

<details>
<summary>Older builds used a different name</summary>

Before this scheme, the app name carried a `Roblox` prefix and the timestamp was in your **local** timezone:

```
RobloxStudio-20260912-143022-8814.log
```

Those files are still readable, they just need you to know which timezone the machine was in.

</details>

## What is in one

Each log starts with a header giving your TuxBlox version, how it was started, the exact command, and your kernel and system information. That header alone answers most of the first questions in a bug report.

After the header comes whatever the compatibility layer had to say during the session.

At the end, Roblox's own logs from that same session are appended, each introduced by a line naming the file it came from:

```
=== ROBLOX LOG: 0.680.0.6800446_20260912T143107Z_Studio_A1B2C.log ===
```

Roblox's names follow their own pattern, which is worth being able to read:

```
0.680.0.6800446_20260912T143107Z_Studio_A1B2C.log
│               │                │      └─ random, so two logs never collide
│               │                └─ Studio or Player
│               └─ when it started, in UTC
└─ the Roblox version
```

So one file has both halves: what TuxBlox saw and what Roblox saw, from the same session, in order.

## Reading one

You do not need to understand all of it. Useful things to look for:

| Look for | Usually means |
|---|---|
| `err:` | Something the layer considers wrong |
| `Exception` | A crash, often with the reason on the same line |
| `Vulkan` or `vulkan` | Graphics driver trouble |
| `wine: failed to open` | Something tried to run a program that is not there |

The last twenty or so lines before the log stops are almost always the interesting part.

```bash
tail -50 "$(ls -t ~/.tuxblox/logs/* | head -1)"
```

## Turning on detailed logging

Logging is deliberately quiet by default, because it costs real frame time. To get more, set `TUXBLOX_LOG=1` in **Environment variables** in Settings, then reproduce the problem.

For even more, you can name Wine debug channels, for example `TUXBLOX_LOG=+seh,+loaddll`.

> [!WARNING]
> Turn it off again when you are done. Verbose channels can slow Roblox to a crawl and produce gigabytes of log. `+relay` in particular will make the session unusable.

## Before you share a log

Logs are written for debugging, not for publishing. Before attaching one:

- Your Linux username appears in file paths throughout. Fine for most people, worth knowing about.
- Place and experience names you opened appear in Roblox's half of the log.

> [!CAUTION]
> Never share your virtual drive, `~/.tuxblox/runtime/`. It can contain your Roblox session cookies, which is equivalent to handing over your account. The log file is safe. The drive is not.

## Crash reports

When Roblox exits badly and **Send crash reports** is on, TuxBlox sends a report containing the exit code, your Roblox and TuxBlox versions, basic system information, and the session log.

That is the whole list. It is sent only on a bad exit, never during normal use, and you can turn it off in Settings. The full policy is at [tuxblox.net/privacy](https://tuxblox.net/privacy).

A crash report is not a bug report. Nobody is reading them one by one and following up. If you want your problem looked at, [report it](reporting-bugs.md).

## Useful system information

When reporting a bug, these four commands cover nearly everything anyone will ask:

```bash
uname -r                                  # kernel
cat /etc/os-release | head -2             # distribution
lspci -k | grep -A3 -i vga                # graphics card and driver
~/.tuxblox/compat/main --version          # TuxBlox version
```

Also worth mentioning: your desktop environment, and whether you are on Wayland or X11.

```bash
echo "$XDG_CURRENT_DESKTOP $XDG_SESSION_TYPE"
```

## Clearing old logs

Logs are never deleted automatically. If the folder has grown:

```bash
rm ~/.tuxblox/logs/*.log
```

Nothing depends on old logs, so this is safe.
