# ADR 0004: Use the "Minimal SPIFFS" partition scheme

- Status: Accepted
- Date: 2026-10-11

## Context

Measured with arduino-cli and ESP32 core 3.3.12 for `esp32c3`
(USB CDC on boot enabled):

| Build | Flash used | Default app partition (1,310,720 bytes) |
| ----- | ---------- | --------------------------------------- |
| `clock.ino` alone | 1,274,489 bytes | 97% |
| Eyes-only `MochiEyes.ino` (U8g2) | 378,334 bytes | 28% |
| Merged firmware | 1,351,143 bytes | 103%: "text section exceeds available space" |

The clock alone already used 97% of the default partition. Most of that is
the Wi-Fi stack, TLS (for HTTPS), the HTTP client and the web server, not the
application code.

## Decision

Build with **Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)**
(`PartitionScheme=min_spiffs`). This scheme keeps two app slots, so
over-the-air updates remain possible later.

## Consequences

- The firmware has room to grow: see the README's Evaluation section for the
  current percentage.
- Everyone flashing the board must change one menu setting. The requirement is
  stated in the sketch header, the README, the CI workflow and this ADR.
- SPIFFS shrinks to 128 KB. Nothing uses SPIFFS: settings and notes live in NVS
  (`Preferences`), which has its own partition and is unaffected.
- Changing the partition table erases the old app, but not NVS, so saved
  Wi-Fi and notes survive.

## Alternatives considered

- **Huge APP (3MB No OTA).** More space, but it removes the second app slot
  and with it any future OTA update.
- **Cut features to fit 1.2 MB.** Dropping HTTPS would lose the crypto page
  (CoinGecko requires TLS); dropping the web server would lose the controller.
  Both are core features.
