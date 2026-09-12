# Wine, as used by TuxBlox

This folder is a fork of [Wine](https://www.winehq.org/), maintained directly
inside this repository rather than as a submodule. It is licensed under the
**LGPL version 2.1**, the same as Wine itself — see
`third_party_licenses/wine/` for the full licence text and the list of Wine's
authors.

It is not a general-purpose Wine build. It is tuned specifically for running
the Roblox Client and Roblox Studio, and changes here are made with that one
goal in mind.

## How this fork is patched

Everything else under `compat/` that comes from somewhere else is a submodule,
and TuxBlox's changes to those live in `compat/patches/` and are applied to a
copy at build time, so the checkout stays clean.

Wine is the exception. It is patched **in place**: edit the files here
directly and commit the change. `build.sh` deliberately refuses a
`compat/patches/wine` directory for that reason.
