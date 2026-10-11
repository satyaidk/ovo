# ADR 0007: Credentials live in a git-ignored `secrets.h`

- Status: Accepted
- Date: 2026-10-11

## Context

`clock.ino` had the home Wi-Fi name and password as `#define`s in the sketch.
This repository is meant to be public, and anything committed to a public
repository should be treated as published, even if it is deleted later.

## Decision

- `secrets.example.h` (committed) holds placeholder values and explains what
  each one is.
- `secrets.h` (git-ignored) holds the real values. The sketch includes it
  when it exists:

  ```cpp
  #if __has_include("secrets.h")
    #include "secrets.h"
  #else
    #include "secrets.example.h"
  #endif
  ```

- Without `secrets.h` the firmware still builds. It fails to join Wi-Fi and
  opens its hotspot, where the real network can be entered from the web page
  (saved to NVS, not to source code).
- The original sketch that contained the password was removed after the
  merge, so it never reaches the repository.

## Consequences

- A fresh clone builds in CI and on a new machine with no extra step.
- The person who owns the hardware keeps a working local build.
- If a real password was ever pushed before this change, it must be rotated:
  removing it from history does not remove it from existing clones.

## Alternatives considered

- **Provisioning only through the hotspot.** Avoids secrets in files
  entirely, but makes the first boot slower for the owner during development.
- **Build flags (`-DWIFI_PASS=...`).** Works with arduino-cli but not with
  the Arduino IDE without editing platform files.
