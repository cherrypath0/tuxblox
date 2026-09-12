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

## Files here that look removable but are not

Two files in this folder look like documentation and are not:

- **`AUTHORS`** is compiled into `shell32` as a resource for Wine's About
  dialog (see `dlls/shell32/shell32.rc`). Deleting it makes `configure` fail
  with "could not create Makefile". A copy also lives in
  `third_party_licenses/wine/`, which is for licence purposes only — it is not
  a replacement for this one.
- **`VERSION`** is read by `configure.ac`, which pulls the Wine version out of
  it with a regular expression. Deleting it breaks `configure` outright, and
  pointing it at the repository's own `VERSION` file does not work because the
  format does not match.
