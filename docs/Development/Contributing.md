# Contributing to TuxBlox

Thanks for your interest in contributing to TuxBlox! This document covers how to get set up, our conventions, and the rules around contributions given the project's licensing structure.

Before reading this or contributing to TuxBlox, it is **highly recommended** to check the [**Building From Source**](https://docs.tuxblox.net/Development/Building%20From%20Source) and the [**Repository Structure**](https://docs.tuxblox.net/Development/Repository%20Structure) documentation, it will get you up-to-date on how we build TuxBlox and how it is structured.

## Getting Started

There are some rules to contributing that you must follow.

## Submitting Changes

1. Open an issue first for anything non-trivial (new features, architectural changes) so it can be discussed before you invest time.
2. Keep pull requests focused, one logical change per request.
3. Write clear commit messages (imperative mood, e.g. "Fix launcher crash on missing config").
4. Reference the related issue number in your pull request description, if applicable.
5. Be responsive to review feedback. Pull requests that go stale without updates may be closed.

## What We Won't Accept

To keep TuxBlox on solid legal and ethical footing:

- **No anti-cheat bypasses, memory modification, or game tampering.** TuxBlox aims to run the official Roblox client by providing a high-fidelity Windows API runtime environment. We do not accept contributions that disable anti-cheat modules, inject gameplay cheats, or tamper with checks that detect actual cheat behavior (memory editing, script/DLL injection, input automation, packet manipulation, altered game logic or results).
  Platform compatibility hooks are permitted, including hiding Wine/Proton environment fingerprints (e.g. `wine_get_version`-style exports, Wine registry keys, VM/hypervisor CPUID bits, host command-line paths) that Hyperion or other checks use purely to detect *"is this Wine/Linux"* rather than *"is this player cheating."* The line is intent, not mechanism: a patch that makes the unmodified official client run as intended on Linux — with no capability a Windows user doesn't also have — is in scope, even if it touches the same API surface an anti-cheat also queries. A patch that suppresses detection of actual tampering, or that grants any capability a normal Windows client wouldn't have, is not.
- **No cheat, exploit, or automation tooling** (aimbots, script injection, etc.), even if framed as a "plugin" or "optional feature."
- **No inclusion of Microsoft-owned binaries, DLLs, or other proprietary redistributables.** Any Windows API behavior should be reimplemented (Wine-style), not shipped as extracted proprietary files.
- **No code copied from sources incompatible with GPLv3 or LGPLv2.1** (e.g. code under a "no derivatives" or source-available-but-non-OSI license) without explicit written permission from the original author, provided to maintainers.

Contributions that violate the above will be closed without merge, regardless of intent.

## Reporting Bugs

> **NOTE: Bug reports for forks of TuxBlox will not be maintained or triaged by TuxBlox developers.** If you're running a modified/forked version of TuxBlox, please report issues to that fork's maintainers instead. We can only support bugs reproducible on unmodified, official TuxBlox builds.

Please include:
- Your Linux distribution and kernel version
- TuxBlox version
- Steps to reproduce
- Relevant logs (redact any personal account information)

## Reporting Security Issues

If you find a security vulnerability, please **do not open a public issue**. Instead, use the appropriate contacting ways for security purposes by checking our [security.txt](https://tuxblox.net/.well-known/security.txt)

## Questions?

Open a discussion thread, create an issue, or join our community on [Discord](https://tuxblox.net/discord) if anything here is unclear. Happy to help new contributors get oriented.