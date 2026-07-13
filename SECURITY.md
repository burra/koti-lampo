# Security Policy

This is a hobbyist preservation and reverse-engineering project for a
decades-old Valmet Kotilämpö HVAC controller. It ships documentation, firmware
ROM dumps, and analysis tooling — not a networked or production service — so the
security surface is small.

## Reporting a vulnerability

If you find a security issue (for example, in the helper scripts under
`ghidra/`, or a supply-chain concern in the GitHub Actions workflows), please
report it privately using GitHub's
[private vulnerability reporting](https://github.com/burra/koti-lampo/security/advisories/new)
rather than opening a public issue.

If private reporting is unavailable, open a regular issue but omit any details
that would let the problem be exploited before a fix is available.

Please include:

- what the problem is and where (file / workflow / commit),
- how to reproduce it,
- the potential impact.

There is no formal SLA, but reports will be acknowledged and addressed on a
best-effort basis.

## Scope

Out of scope: the third-party materials documented in [`NOTICE`](NOTICE) (the
original Valmet firmware, manuals, and schematics). Reports about the security
of the original 1980s controller hardware/firmware itself are not actionable
here.
