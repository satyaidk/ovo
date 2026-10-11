# ADR 0005: An Arduino-free, unit-tested touch gesture recognizer

- Status: Accepted
- Date: 2026-10-11

## Context

One touch pad now carries many meanings: 1, 2, 3 or 4+ taps, a hold
(petting, or a refresh on the clock), and a 5-second hold to switch faces.
`clock.ino` only had to tell a tap from a 1.5-second press, which a few lines
in `serviceTouch()` handled. With multi-tap the timing rules interact:

- a single tap cannot be reported until we know no second tap is coming;
- a hold must not also count as a tap;
- the 5-second hold must not end with a "hold released" action;
- noise and finger bounce must not create phantom taps.

These bugs are easy to write and hard to see by poking a pad.

## Decision

Put the logic in `src/touch/TouchGestures.h`, a header-only C++ class with no
Arduino dependency. It is fed `(pinLevel, millis)` every loop and returns at
most one event: `Press`, `Taps` (with a count), `HoldStart`, `HoldEnd`
(with a duration) or `ModeHold`. Timings are in a `Config` struct:

| Setting | Default | Meaning |
| ------- | ------- | ------- |
| `debounceMs` | 25 | Level must be stable this long |
| `tapMaxMs` | 350 | Longest press that is still a tap |
| `tapGapMs` | 320 | Longest pause between taps of one burst |
| `holdMs` | 650 | Press length that starts a hold |
| `modeHoldMs` | 5000 | Press length that switches faces |

`tests/host/test_touch.cpp` plays scripted touch timelines through it
(1 ms steps, like `loop()`): tap counts 1 to 5, single-tap latency, slow taps,
holds, the mode hold, glitches, dropouts, tap saturation and the 49-day
`millis()` wrap. CI runs it with coverage.

## Consequences

- Measured: 48 checks pass and gcov reports 100% line coverage of
  `TouchGestures.h` (42 of 42 lines).
- A single tap costs `tapGapMs` (320 ms) of latency, the price of supporting
  double taps. The `Press` event fires immediately so the eyes can react at
  once (they widen slightly) while the gesture resolves.
- `MochiEyes.ino` only maps events to actions, which keeps the behaviour easy to
  read: `onEyesGesture()` and `onClockGesture()`.

## Alternatives considered

- **OneButton or similar library.** Adds a dependency and its hold/multi-click
  semantics do not match the 5-second mode switch exactly.
- **Inline state machine in the sketch.** Not testable without hardware.
