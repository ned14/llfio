# Security Policy

## Supported Versions

LLFIO does not maintain long-term-support branches. Security fixes are made
against the latest `develop` branch and included in the next release. If you
are not on the latest release, please upgrade before reporting an issue you
have not verified against current `develop`.

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub
issues.**

Instead, use GitHub's private vulnerability reporting:

1. Go to the [Security tab](https://github.com/ned14/llfio/security) of this
   repository.
2. Click **"Report a vulnerability"** under Security Advisories.
3. Provide as much detail as you can: affected version/commit, platform,
   a minimal reproduction, and the potential impact.

You should receive an acknowledgement within a reasonable time. Please allow
time for a fix to be developed and released before any public disclosure
(coordinated disclosure).

## Scope

As a low-level file I/O and filesystem library, issues of particular interest
include (but are not limited to):
- Memory safety issues (buffer overflows, use-after-free, etc.) reachable via
  the public API.
- TOCTOU / race-free filesystem guarantees being violated.
- Path handling issues that could lead to unintended file access outside the
  intended scope (e.g. path traversal in the race-free filesystem algorithms).

General correctness bugs that do not have security impact should be reported
as normal [GitHub issues](https://github.com/ned14/llfio/issues) instead.
