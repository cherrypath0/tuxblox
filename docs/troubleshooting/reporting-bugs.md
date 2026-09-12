# Reporting Bugs

> [!CAUTION]
> **Found a security issue?** Do not report it publicly. Email **security@tuxblox.net** instead.

## Before you report

1. Check [Common Problems](common-problems.md). Most reports have an answer there.
2. Check [Known Issues](known-issues.md). We may already know.
3. Try it on the latest version. It may already be fixed.

> [!NOTE]
> We cannot look into bugs in modified or forked builds of TuxBlox. If you are running a fork, report it to that fork's maintainers. We can only help with unmodified, official builds.

## What to include

The more of this you can give, the faster it gets fixed. You do not need all of it.

**About your system:**

- Distribution and version
- Kernel version (`uname -r`)
- Desktop environment (GNOME, KDE Plasma, Hyprland, and so on)
- Wayland or X11
- Graphics card and driver version

**About TuxBlox:**

- Your TuxBlox version, from the About tab
- Your update channel
- Whether you have any FastFlags set, and which
- Anything in the Environment variables box

**About the bug:**

- What you expected to happen
- What actually happened
- Steps to make it happen again
- Which Roblox experience or place, if it only happens in one
- The log file from `~/.tuxblox/logs`

[Logs and Diagnostics](logs.md) has commands that collect most of the system information in one go.

> [!WARNING]
> Attach the log file. Never attach your virtual drive, `~/.tuxblox/runtime/`. It can contain your Roblox session cookies.

## A good report looks like this

> **Studio's Toolbox is blank on Fedora 41**
>
> Fedora 41, kernel 6.14.3, KDE Plasma on Wayland, AMD RX 6700 XT with Mesa 25.0.2.
> TuxBlox 2.1.0 stable, no FastFlags, nothing in the environment box.
>
> Opening the Toolbox in Studio shows an empty grey panel. Searching does nothing. Every other panel works.
>
> Happens every time, on a brand new empty place. Started after updating to 2.1.0, was fine on 2.0.4.
>
> Log attached.

That report can be acted on immediately. "Toolbox broken pls fix" cannot.

## Where to report

### Discord

[tuxblox.net/discord](https://tuxblox.net/discord), in the bugs channel under Support.

Best for anything you are not sure is a bug, and for anything where a back and forth will help.

### GitLab

[gitlab.com/cherrypath0/tuxblox/-/issues](https://gitlab.com/cherrypath0/tuxblox/-/issues)

Best for a clear, reproducible bug you can describe in one go. Search existing issues first.

### GitHub

[github.com/cherrypath0/tuxblox/issues](https://github.com/cherrypath0/tuxblox/issues)

The same project. Every change is pushed to GitLab and GitHub together, so the code is identical on both. Use whichever you already have an account on, there is no need to post in both. Search existing issues first.

## After reporting

Please be patient. TuxBlox is a small project and it may be a while before someone gets to your report. Being asked for more information is normal and usually means somebody is actually looking.
