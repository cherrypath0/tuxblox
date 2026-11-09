# Contributing to TuxBlox

Thanks for your interest in contributing to TuxBlox! This document covers how to get set up, our conventions, and the rules for contributions.

## Licensing

TuxBlox is a multi-licence repository. Please read this section before contributing.

| Path | Licence |
|---|---|
| Everything outside `compat/` (launcher, installer, studio-mcp, shared code, tooling) | **GPLv3** |
| `compat/` (the TuxBlox compatibility layer, based on Wine and Proton) | **LGPLv2.1**, inherited from Wine and Proton |
| `compat/tuxblox/` and `compat/webkitgtk/bundle/` | **GPLv3** (TuxBlox's own code) |
| `third_party_licenses/` | Licence texts of bundled third-party code |

- Your contribution must be compatible with the licence of the place you're contributing to.
- TuxBlox's own code and `compat/` build into **separate binaries** that communicate at runtime. They are not linked into one binary. Keep this separation intact, because it's what lets the two parts carry different licences.
- **Copyright header:** every new source file that is TuxBlox's own code (everything outside `compat/`, plus `compat/tuxblox/` and `compat/webkitgtk/bundle/`) needs the project's GPLv3 copyright header. Copy it from an existing file in the same folder. **Never add it anywhere else under `compat/`**, since that code carries its own licensing and upstream header conventions.
- **Do not modify `third_party_licenses/`.** The only exception is when you add or remove third-party code: then add or remove its licence folder to match. Each project, including Wine and Proton, has its own folder.
- Do not copy in code under a licence that isn't compatible with GPLv3 or LGPLv2.1 (for example "no derivatives" or source-available, non-OSI licences) unless the original author has given written permission and you pass it on to the maintainers.

By opening a pull request, you agree that your contribution is licensed under the licence of the component you're contributing to, and that you have the right to submit it under that licence.

## Building

> [!WARNING]
> A first build takes one to two hours and needs 50 GB of disk. You only need to build from source if you want to change something.

### Requirements

| | |
|---|---|
| **Processor** | A modern x86-64 processor. More cores means a shorter build. |
| **Memory** | 16 GB of RAM or more |
| **Storage** | 50 GB free |
| **Packages** | `python3`, `podman`, `curl`, `git`, `zstd`, `gcc`, `uidmap` |

Building inside WSL or a virtual machine is not supported.

> [!IMPORTANT]
> **Podman, not Docker.** Parts of the build (`build.sh`, `launcher/build.sh`, `installer/build.sh` and the helper scripts in `shared/tools/`) run inside containers using `podman run --userns=keep-id`, a rootless flag Docker doesn't have. Docker is fine for anything else in the repository, but it won't work for these steps.

### Build

```bash
git clone https://github.com/<you>/tuxblox
cd tuxblox
./build.sh --log --nodebug
```

- **Don't run it with `sudo`.** The script detects your package manager and installs anything missing, asking for your password only if it needs to.
- It asks which release channel to build for. Press Enter to accept the default, `experimental`.
- **Always use `--log`.** It writes everything to `build.log`, and when a build fails it prints the lines around the last error. Without it, a failure leaves nothing to look at.

| Option | What it does |
|---|---|
| *(none)* | Full build, with each step's output shown |
| `--nodebug` | Quieter: only summaries, not every step's output |
| `--log` | Also write everything to `build.log`, and diagnose failures automatically |
| `--stage-only` | Re-package the existing build without rebuilding it |
| `--local` | Build and package, upload nothing (only matters for maintainers) |

| Build-time variable | What it does |
|---|---|
| `TUXBLOX_MAKE_JOBS` | How many compile jobs to run at once, instead of one per core. Lower it if you run out of memory. |
| `TUXBLOX_DEV_TOOLS` | Include the development tooling in the build |
| `TUXBLOX_HEADLESS_ONLY` | Build the installer without its window, for a machine with no display |
| `TUXBLOX_BUILD_VERSION`, `TUXBLOX_CHANNEL` | Set the release version and channel yourself. Needed when building offline, since working out the version needs `setup.tuxblox.net`. |

```bash
TUXBLOX_MAKE_JOBS=4 ./build.sh --log --nodebug
```

Later builds are much faster, because only what changed is compiled again. Every build wipes and recreates `build/`, so don't keep anything there by hand.

### Running your build

`build/` has the same layout as an install, so you can run it in place without touching your real install at `~/.tuxblox`:

```bash
./build/TuxBloxLauncher
```

It uses `build/runtime` as its virtual drive and `build/logs` for its logs.

To start a program through the compatibility layer directly, skipping the launcher:

```bash
TUXBLOX_PREFIX="$PWD/build/runtime" \
  ./build/compat/main run \
  "$PWD/build/runtime/pfx/drive_c/users/<your account>/AppData/Local/Roblox/Versions/version-<id>/RobloxStudioBeta.exe"
```

Two things that will cost you an afternoon if you don't know them:

- **The path must be absolute, and the program must be inside `drive_c`.** Anything else exits instantly with no message, which looks exactly like a crash but isn't one.
- **Read the output in the terminal.** Redirecting it to a file leaves the program with nothing to write to, and you get an empty file.

### Tests

The launcher and installer builds run a fresh-install smoke test, and a failure stops the build. `TUXBLOX_SKIP_SMOKE=1` skips it while you iterate, but **never skip it for a build you're submitting**. Unit tests live in `launcher/tests/`, `installer/tests/`, `shared/*/tests/` and `shared/tools/tests/`. Add or update tests there when you change the code they cover.

## Repository Layout

| You want to change | Go to |
|---|---|
| Something in the launcher window | `launcher/src/ui_adw/` |
| How a setting behaves | `launcher/src/settings.cpp` |
| The install or update flow | `installer/src/` |
| Code shared between components | `shared/` |
| How Roblox is started | `compat/tuxblox/src/launch/` |
| How the virtual drive is built | `compat/tuxblox/src/prefix/` |
| Windows behaviour Roblox depends on | `compat/wine/` |
| Studio's login screen or Toolbox | `compat/webkitgtk/bundle/` |
| A dependency such as DXVK or vkd3d | `compat/patches/<package>/` |

**Don't commit changes inside the dependency submodules** in `compat/submodules/`. Put the changed file in `compat/patches/<package>/<path>` instead, and the build copies it over its copy of the source. The exception is `compat/wine/`, TuxBlox's own Wine fork, which is edited in place.

## Code Style

- Match the existing style of the file or module you're editing. Local consistency beats global rules.
- Keep functions focused and reasonably sized. Prefer clarity over cleverness.
- Comment non-obvious logic, especially environment setup, the virtual drive and update behaviour. Keep comments short.

## Submitting Changes

1. **Open an issue first** for anything non-trivial (new features, architectural changes), so it can be discussed before you spend time on it.
2. **Open pull requests against `main`.** `main` is the experimental channel; maintainers move changes to `canary` and `stable`.
3. **Keep pull requests focused:** one logical change per pull request.
4. **Write clear commit messages** in the imperative mood, e.g. `Fix launcher crash on missing config`.
5. **Sign off every commit** (see below).
6. **Reference the related issue** in the pull request description, if there is one.
7. **Respond to review feedback.** Pull requests that go stale without updates may be closed.

### Signing off commits

Every commit must end with a `Signed-off-by:` line with your real name or known handle and email:

```
Signed-off-by: Your Name <you@example.com>
```

`git commit -s` adds it for you. By signing off, you certify the [Developer Certificate of Origin](https://developercertificate.org/): that you wrote the change, or otherwise have the right to submit it under the project's licence.

### Changelogs

**Do not edit** `compat/changelogs.md`, `launcher/changelogs.md`, `installer/changelogs.md` or `studio-mcp/changelogs.md`. Maintainers write changelogs at release time. Describe your change in the pull request instead.

## Using AI Tools

AI tools are welcome for any part of the repository. **Read [AGENTS.md](AGENTS.md) for the repository's full AI policy** (`CLAUDE.md` links to the same file). In short, for you as a contributor:

- **You must fully review all AI-written code** and be able to explain and defend every line of it in review.
- **You are fully responsible** for everything in your contribution, whoever or whatever wrote it.
- **AI must not be listed as an author.** Do not add `Signed-off-by:`, `Co-authored-by:` or any other authorship trailer naming an AI model or tool. Your own `Signed-off-by:` is still required. Many tools add a `Co-authored-by:` line automatically, so turn that off in your tool's settings.
- **Optionally, you may disclose AI help** with an `Assisted-by:` trailer. It does not claim authorship:

  ```
  Assisted-by: AGENT_NAME MODEL_VERSION ([TOOL])
  ```

  For example:

  ```
  Assisted-by: Claude Opus 5.5 (Claude Code)
  Assisted-by: GPT-Astra 6 (Codex)
  ```

- **Don't commit AI tool artifacts** (specs, plans, notes, scratch files). Keep them in `workspace/`, which is gitignored, and don't change `.gitignore` to hide them.

## Roblox, Studio and Hyperion

TuxBlox runs the **official, unmodified** Roblox client by providing an accurate Windows API environment. Hyperion is the anti-tamper software Roblox uses to protect Roblox Player. It is used only on the Windows clients.

All contributions must follow the [Roblox Terms of Use](https://rbxl.co/tou).

**Compatibility work is welcome.** Making Windows APIs, the loader, the prefix, graphics, input, audio and networking behave as they do on real Windows, so that Roblox Player, Roblox Studio and Hyperion run as intended, is in scope.

The line is **intent, not mechanism**. A change that makes the unmodified official client run as intended on Linux, and gives no capability a normal Windows user doesn't also have, is in scope, even if it touches API surface that anti-tamper software also uses. A change that hides real tampering, or gives any capability a normal Windows client wouldn't have, is not.

When you write about compatibility work in code comments, commit messages, issues or pull requests, describe the **Windows behaviour you're fixing**, not what Roblox or Hyperion does with it.

## What We Won't Accept

To keep TuxBlox on solid legal and ethical footing, the following will be closed without merge, whatever the intent:

- **Bypassing or tampering with the Roblox client, Roblox Studio or Hyperion.** This includes patching, hooking, injecting into or modifying their binaries or memory, and disabling, spoofing or getting around their checks.
- **Discussing findings about Hyperion**: how it works internally, or what it checks, detects or reports. This applies anywhere in the project: code, comments, commits, issues, pull requests, discussions and docs.
- **Decompiling, disassembling or reverse engineering any Roblox binary** (Player, Studio, Hyperion, installers, DLLs or any other file Roblox ships), or using leaked, decompiled or otherwise non-public Roblox code.
- **Cheat, exploit or automation tooling**, such as aimbots, executors, script or DLL injection, memory editing, input automation, packet manipulation or HWID spoofing, even if presented as a "plugin" or "optional feature".
- **Microsoft-owned binaries, DLLs or other proprietary redistributables.** Windows API behaviour must be reimplemented (as Wine does), not shipped as extracted proprietary files.
- **Code under incompatible licences** (see [Licensing](#licensing)).

## Reporting Bugs

> [!NOTE]
> **TuxBlox developers do not triage bug reports for forks of TuxBlox.** If you're running a modified or forked version, please report issues to that fork's maintainers. We can only support bugs that reproduce on unmodified, official TuxBlox builds.

Please include:

- Your Linux distribution and kernel version
- Your TuxBlox version
- Steps to reproduce
- Relevant logs (redact any personal or account information, including Roblox cookies)

## Reporting Security Issues

If you find a security vulnerability, **do not open a public issue or pull request**. Report it privately by email to **security@tuxblox.net**, following the rules in our [security.txt](https://tuxblox.net/.well-known/security.txt). See [SECURITY.md](SECURITY.md) for the other private reporting options.

## Questions?

Open a discussion thread, create an issue, or join our community on [Discord](https://tuxblox.net/discord) if anything here is unclear. We're happy to help new contributors get started.