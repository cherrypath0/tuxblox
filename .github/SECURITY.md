# Security Policy

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Publicly disclosing a vulnerability before it's fixed can put users at risk. Instead,
please report it privately using one of the following methods:

- Check our [security.txt](https://tuxblox.net/.well-known/security.txt) for the
  current contact address.
- (Optional) Use GitHub's [private vulnerability reporting](../../security/advisories/new)
  feature for this repository.

Please include as much of the following as you can:

- A description of the vulnerability and its potential impact
- Steps to reproduce, or a proof-of-concept if available
- The TuxBlox version and component affected (launcher vs. `ProtonSource/`)
- Your Linux distribution and kernel version, if relevant

## Scope

This policy covers:

- TuxBlox's own launcher code (GPLv3-licensed portions of this repository)
- TB-Proton, our modified Proton build (`ProtonSource/`, LGPLv2.1-licensed)

Vulnerabilities in upstream Wine, Proton, DXVK, or vkd3d-proton that are **not**
introduced by TuxBlox-specific modifications should be reported upstream to those
projects directly, though we're happy to help route reports if you're unsure where
an issue originates.

## What to Expect

We aim to acknowledge reports within a reasonable timeframe and will work with you
to understand and address the issue. We ask that you give us a reasonable amount of
time to address a vulnerability before any public disclosure.

## Out of Scope

- Vulnerabilities that require a user to already have a compromised or maliciously
  modified TuxBlox build installed
- Reports concerning forks or modified builds of TuxBlox (please report those to the
  fork's maintainers)
- Social engineering, phishing, or physical security issues unrelated to the software
  itself

Thank you for helping keep TuxBlox and its users safe.