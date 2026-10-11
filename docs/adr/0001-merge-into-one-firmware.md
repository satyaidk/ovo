# ADR 0001: Merge the clock and the eyes into one firmware

- Status: Accepted
- Date: 2026-10-11

## Context

The device started as two separate sketches for the same hardware: an info
terminal (`clock.ino`: clock, weather, crypto, notes, Wi-Fi, web page) and an
eyes-only sketch (`MochiEyes.ino`: the eye engine and a Serial console).
Switching between them meant re-flashing. Both used the same OLED pins
(SDA 20, SCL 21), so the hardware already supported a single program.

Arduino compiles every `.ino` in a sketch folder into one translation unit,
so the two files could not simply sit side by side: both define `setup()` and
`loop()`.

## Decision

Build one firmware, `MochiEyes.ino`, with two *faces* (eyes and clock) and a
5-second touch hold to switch. The clock code keeps its behaviour and its
storage namespace (`terminal`), so a saved note and Wi-Fi details survive the
upgrade. The original sketches were removed once their behaviour was merged
(one of them also contained a real Wi-Fi password).

Everything that is flashed lives in one folder, `firmware/MochiEyes/`, so it
is obvious what goes onto the board; docs, tests and tools sit outside it.

The application stays in a single `.ino` as requested; only code with a clear
reason to be separate lives elsewhere:

- `src/eye/`: the existing engine, unchanged, Arduino-free.
- `src/touch/`: the gesture recognizer, Arduino-free so it can be tested on a
  PC ([ADR 0005](0005-arduino-free-touch-gesture-recognizer.md)).
- `src/web/WebUi.h`: generated from `web/index.html`
  ([ADR 0006](0006-gzipped-web-ui-in-flash.md)).

## Consequences

- One flash, one web page, one command language for everything.
- The combined program no longer fits the default partition
  ([ADR 0004](0004-minimal-spiffs-partition.md)).
- `MochiEyes.ino` is long (about 1,500 lines). Its sections follow the order
  hardware, data, behaviour, rendering, input, web, setup/loop, and every
  section starts with a one-line marker, so it can be read top to bottom.

## Alternatives considered

- **Keep two firmwares.** No code risk, but re-flashing to change modes is
  not a product.
- **Split the `.ino` into many `.cpp` modules.** Cleaner for a large team, but
  the request was for one file, and shared globals (display, engine, state)
  would need extern declarations across several headers, which is harder for a
  beginner to follow.
