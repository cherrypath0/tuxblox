<!--
Keep merge requests focused: one logical change per request. See CONTRIBUTING.md
for the full guidelines, including what we can't accept.

For anything non-trivial (new features, architectural changes), please open an
issue first so it can be discussed before you invest time.
-->

### What this changes

<!-- What does it do, and why? If it fixes an issue, link it: Closes #123 -->

### How it was tested

<!-- What you actually ran, and what it reported. "Builds clean" is not testing.
     Say plainly if part of it is unverified, and why. -->

### Licensing

<!--
TuxBlox is a two-license repository and the boundary has to stay intact:

  - Everything outside compat/ is GPLv3 (TuxBlox's own launcher tooling).
  - compat/ is LGPLv2.1, inherited from Wine/Proton. The exception is
    compat/tuxblox/, which is TuxBlox's own C++ and is GPLv3 like the rest.

The two are separate compiled artifacts that talk at runtime, never statically
linked into one binary. That separation is what lets the project carry both
licenses cleanly, so please don't blur it.

Replace [ ] with [x].
-->

- [ ] This touches only one side of the GPLv3 / LGPLv2.1 boundary, or keeps the two separate where it touches both
- [ ] I have the right to submit this under that component's license, and agree it is licensed under it
- [ ] No Microsoft-owned binaries, DLLs or other proprietary redistributables are included
- [ ] No code copied from sources incompatible with GPLv3 or LGPLv2.1

### Checklist

- [ ] Keeps to one logical change
- [ ] Non-obvious logic is commented, especially around compatibility-layer flags, environment setup, or update mechanics
- [ ] Matches the style of the files it edits

/label ~"needs review"
