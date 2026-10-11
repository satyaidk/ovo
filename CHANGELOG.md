# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-10-11

First release of the merged firmware. It combines the info terminal
(`clock.ino`) and the earlier eyes-only sketch into one program, named OvoEyes.

### Added

- Two faces on one OLED: the Ovo eyes and the clock pages. Holding the touch
  pad for 5 seconds switches between them, with a progress bar on screen and a
  shoji-door transition.
- Touch reactions for the eyes: 1 tap glances at the pad, 2 taps smile,
  3 taps get grumpy, 4 or more taps get dizzy (eyes orbit in opposite
  directions), and holding the pad is petting.
- Auto-sleep: drowsy after 2 minutes without interaction, asleep after 5;
  any tap wakes the eyes.
- Clock gestures: 2 taps go back a page, 3 taps return to the clock face.
- Redesigned web controller in an indigo-dyed Japanese style, with light and
  dark themes, an ensō gaze pad, and a live mirror of the OLED.
- JSON web API (`/api/state`, `/api/eye`, `/api/mode`, and more), documented in
  `docs/API.md`.
- Settings saved on the device: sound, auto-sleep, and which face to show after
  a restart.
- `src/touch/TouchGestures.h`, an Arduino-free tap and hold recognizer with
  48 host-test checks.
- Tools: `tools/embed_web.py` (gzips the page into the firmware),
  `tools/preview/` (preview the page with eye frames rendered on a PC).
- CI that compiles the firmware, runs host tests, and checks the embedded page.

### Changed

- Everything that is flashed now lives in one folder, `firmware/OvoEyes/`;
  docs, tests and tools sit outside it. The two original sketches were removed
  after merging.
- Weather and price downloads run in a FreeRTOS task, so the eyes keep moving
  during HTTPS requests.
- The eyes now render into the Adafruit SSD1306 buffer instead of U8g2, so one
  display library serves both faces.
- The local hostname is now `ovo.local` (was `terminal.local`).
- Partition scheme is now **Minimal SPIFFS (1.9MB APP with OTA)**. The merged
  firmware does not fit the default 1.2 MB app partition.
- The I2C pin search no longer probes the touch and buzzer pins.

### Security

- Wi-Fi credentials moved from the sketch into the git-ignored `secrets.h`
  (template: `secrets.example.h`).

[Unreleased]: https://github.com/satyaidk/OvoEyes/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/satyaidk/OvoEyes/releases/tag/v1.0.0
