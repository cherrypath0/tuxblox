# Building From Source

> [!WARNING]
> **Most people should not do this.** A first build takes one to two hours and needs 50 GB of disk. Installing normally takes about a minute and gets you the same thing. Build from source only if you want to change something.

Building has its own requirements, separate from the ones needed just to run TuxBlox.

## Requirements

| | |
|---|---|
| **Processor** | A modern x86-64 processor. More cores means a shorter wait. |
| **Memory** | 16 GB of RAM or more |
| **Storage** | 50 GB free |
| **Packages** | `python3`, `podman`, `curl`, `git`, `zstd`, `gcc`, `uidmap` |

Building inside WSL or a virtual machine is not supported.

> [!IMPORTANT]
> **Podman, not Docker.** Parts of the build run inside a container using `podman run --userns=keep-id`, a rootless flag Docker does not have. Docker is fine for anything else in the repository, but it will not work for these steps.

## Building

### 1. Clone the repository

```bash
git clone https://gitlab.com/cherrypath0/tuxblox
cd tuxblox
```

The same repository is on [GitHub](https://github.com/cherrypath0/tuxblox) if you would rather clone from there. Every change is pushed to both, so neither one lags behind the other.

### 2. Run the build script

```bash
./build.sh --log --nodebug
```

Do not run this with `sudo`. The script works out which package manager you have and installs anything missing, asking for your password only if it needs to.

It will ask for a version number and a release channel, offering whatever is in the `VERSION` file as the default. Press Enter to accept, or type your own, for example `MyBuild-1.0`.

Then wait. The first build compiles the entire compatibility layer.

### Build options

| Option | What it does |
|---|---|
| *(none)* | Full build, with each step's output shown |
| `--nodebug` | Quieter. Only summaries, not every step's output. |
| `--log` | Also write everything to `build.log`, and diagnose failures automatically |
| `--stage-only` | Re-package the existing build without rebuilding it |

`--log` is worth using every time. Without it there is no build log, so a failure leaves nothing to look at.

### 3. Install what you built

Everything lands in `build/`. To use it:

```bash
mkdir -p ~/.tuxblox
cp -a build/* ~/.tuxblox/
~/.tuxblox/TuxBloxLauncher
```

This does not set up desktop shortcuts or link handlers, so add those yourself if you want them.

A build installed this way still gets normal updates, which will replace it. Switch to the `experimental` channel, or skip updates, if you want to keep your own build.

## Rebuilding

Later builds are much faster, because TuxBlox reuses what it can. Only what actually changed gets compiled again.

A build wipes and recreates `build/` every time, including the virtual drive underneath it, so anything you put there by hand is gone.

## Running what you built, in place

You do not have to copy the build anywhere to try it:

```bash
./launch.sh          # asks which one you want
./launch.sh studio
./launch.sh player
```

This uses the virtual drive inside `build/` rather than your installed one, so experiments cannot damage a working install.

## Versioning

The version comes from the `VERSION` file at the repository root: version on line one, channel on line two. That one file is the only place a version is defined, so the launcher, the installer and the compatibility layer can never disagree about which version they are.

You can skip the prompt by setting it in the environment:

```bash
TUXBLOX_BUILD_VERSION=2.4.1 TUXBLOX_CHANNEL=experimental ./build.sh --log --nodebug
```

`build.sh` writes whatever it settled on back into `VERSION`, so the file keeps itself current.

## When a build fails

Re-run with `--log` if you did not already. On a real failure the script searches `build.log` for the last error and prints the surrounding lines, which is usually enough to see what happened.

If you are stuck, ask in [Discord](https://tuxblox.net/discord) with that section of the log.

## Changing something

Read [Repository Structure](repository-structure.md) first, to know which half of the project you are in. The licensing split matters, and so does which folder a change belongs in.

Then read [Contributing](contributing.md) before opening a merge or pull request.
