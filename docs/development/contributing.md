# Contributing

Thanks for wanting to help out. TuxBlox is a small project and contributions genuinely matter.

The authoritative guide is [`CONTRIBUTING.md`](https://github.com/cherrypath0/tuxblox/blob/main/CONTRIBUTING.md) at the root of the repository. This page is the short version, to help you decide whether to open that file at all.

## Before you start

- [Building From Source](building-from-source.md), to get it compiling locally.
- [Repository Structure](repository-structure.md), to find where your change belongs.

**Open an issue first** for anything non-trivial. A feature or an architectural change is worth discussing before you spend a weekend on it, and it is much better than finding out at review time that it does not fit.

## Submitting a change

1. Fork, clone, branch.
2. One logical change per request. A merge request on GitLab or a pull request on GitHub, either is fine. One that does four unrelated things is four times harder to review.
3. Write commit messages in the imperative mood: "Fix launcher crash on missing config", not "fixed stuff".
4. Reference the issue number if there is one.
5. Stay responsive to review. Requests that go quiet may be closed.

## Code style

- Match the file you are editing. Local consistency beats global rules.
- Keep functions focused. Prefer clarity over cleverness.
- Comment the non-obvious parts, especially anything around environment setup, the virtual drive, or update behaviour. Keep comments short.

Every source file that is TuxBlox's own code needs the project copyright header. That means everything outside `compat/`, plus `compat/tuxblox/` and `compat/webkitgtk/bundle/`. Never add it anywhere else under `compat/`, since that code carries its own licensing and its own upstream header conventions.

## Licensing

By opening a merge or pull request you agree your contribution is licensed the same way the component you are contributing to is: GPLv3 for TuxBlox's own code, LGPLv2.1 for `compat/`.

## What will not be accepted

These are closed without merge regardless of intent.

- **Anti-cheat bypasses, memory modification, or game tampering.** Anything that suppresses detection of actual cheating, or grants a capability a Windows player does not have.
- **Cheat, exploit or automation tooling.** Aimbots, script injection, input automation, even framed as an optional plugin.
- **Microsoft owned binaries or proprietary redistributables.** Windows behaviour gets reimplemented, Wine style, not shipped as extracted files.
- **Code from licenses incompatible with GPLv3 or LGPLv2.1**, without written permission from the author given to the maintainers.

### The line, specifically

Hiding the fact that Roblox is running under Wine is fine. Hiding what a player is doing inside the game is not.

A patch that makes the unmodified official client run properly on Linux is in scope, even when it touches the same interfaces an anti-cheat also reads. A patch that hides tampering, or hands a Linux player something a Windows player does not have, is not.

The distinction is intent, not mechanism: "is this Wine" versus "is this player cheating".

## Reporting security issues

Do not open a public issue. Contact details are in [security.txt](https://tuxblox.net/.well-known/security.txt), or email **security@tuxblox.net**.

## Questions

Ask in [Discord](https://tuxblox.net/discord). New contributors are welcome and someone will help you get oriented.
