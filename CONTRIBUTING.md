![TuxBlox Banner](https://assetdelivery.tuxblox.net/images/png/banner/tuxblox-banner-medium.png)

# Contributing to TuxBlox

Thanks for your interest in contributing to TuxBlox! This document covers how to get set up, our conventions, and the rules around contributions given the project's licensing structure.

## Project Structure & Licensing

TuxBlox is a multi-license repository, make sure to read all of these:

- **TuxBlox's own launcher code** (everything outside **`ProtonSource/`**) is licensed under **GPLv3**. Contributions here must be compatible with GPLv3.
- **`ProtonSource/`** (our modified Proton build) remains licensed under **LGPLv2.1**, inherited from Wine/Proton. Contributions here must comply with LGPLv2.1.
- **`third_party_licenses/`** is the license texts directory for bundled dependencies. Do not modify.

By submitting a pull request, you agree that your contribution is licensed under the same license as the component you're contributing to (GPLv3 for TuxBlox's own code, LGPLv2.1 for `ProtonSource/`), and that you have the right to submit it under that license.

## Getting Started

1. Fork the repository and clone your fork.
2. Install dependencies (see [tuxblox.net/docs](https://tuxblox.net/docs) for current build requirements).
3. Build both components separately:
   - TuxBlox's own code builds into its own binary.
   - `ProtonSource/` builds into its own separate binary.
   These are **separate compiled artifacts** that communicate at runtime, they are not statically linked into a single binary. Please keep this separation intact in any changes you make; it's what allows the two components to carry different licenses cleanly.
4. Run the test suite (see docs) before opening a pull request.

## Code Style

- Keep functions focused and reasonably sized; prefer clarity over cleverness.
- Match the existing style of the file/module you're editing if it differs slightly from the above.
- Comment non-obvious logic, especially anything related to Proton flag handling, environment setup, or update mechanics.

## Submitting Changes

1. Open an issue first for anything non-trivial (new features, architectural changes) so it can be discussed before you invest time.
2. Keep pull requests focused, one logical change per request.
3. Write clear commit messages (imperative mood, e.g. "Fix launcher crash on missing config").
4. Reference the related issue number in your pull request description, if applicable.
5. Be responsive to review feedback. Pull requests that go stale without updates may be closed.

## What We Won't Accept

To keep TuxBlox on solid legal and ethical footing:

- **No code that circumvents, disables, or interferes with Roblox's anti-cheat or client integrity systems.** TuxBlox aims to run the official client faithfully, not to modify or defeat its protections.
- **No cheat, exploit, or automation tooling** (aimbots, script injection, etc.), even if framed as a "plugin" or "optional feature."
- **No inclusion of Microsoft-owned binaries, DLLs, or other proprietary redistributables.** Any Windows API behavior should be reimplemented (Wine-style), not shipped as extracted proprietary files.
- **No code copied from sources incompatible with GPLv3 or LGPLv2.1** (e.g. code under a "no derivatives" or source-available-but-non-OSI license) without explicit written permission from the original author, provided to maintainers.

Contributions that violate the above will be closed without merge, regardless of intent.

## Reporting Bugs

> [!NOTE]
> **Bug reports for forks of TuxBlox will not be maintained or triaged by TuxBlox developers.** If you're running a modified/forked version of TuxBlox, please report issues to that fork's maintainers instead. We can only support bugs reproducible on unmodified, official TuxBlox builds.

Please include:
- Your Linux distribution and kernel version
- TuxBlox version
- Steps to reproduce
- Relevant logs (redact any personal account information)

## Reporting Security Issues

If you find a security vulnerability, please **do not open a public issue**. Instead, use the appropriate contacting ways for security purposes by checking our [security.txt](https://tuxblox.net/.well-known/security.txt)

## Questions?

Open a discussion thread, create an issue, or join our community on [Discord](https://tuxblox.net/discord) if anything here is unclear. Happy to help new contributors get oriented.