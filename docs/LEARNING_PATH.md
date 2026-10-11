# Learning path

A guided route through the code for someone new to embedded C++. Paths
starting with `src/` or `MochiEyes.ino` are inside `firmware/MochiEyes/`. Each step
names the files to read, what to look for, and a small exercise to make the
idea stick. Plan for a weekend; steps 1 to 4 need no hardware.

## Before you start

You should be comfortable with basic C++ (functions, structs, classes) and
have the Arduino IDE installed. You do **not** need to know FreeRTOS, I2C or
HTTP yet; the steps introduce them.

## Step 1: Pixels and the frame buffer

**Read:** `src/eye/EyeRenderer.h` (top comment and `setPixel`).

An OLED of 128×64 pixels is stored as 1024 bytes. Each byte is a vertical
strip of 8 pixels. That is why `setPixel` computes `(y >> 3) * 128 + x` and
then sets bit `y & 7`.

**Exercise:** in `web/index.html`, find `drawFrame()`. It decodes the same
layout in JavaScript. Explain to yourself why both use `>> 3` and `& 7`.

## Step 2: Behaviour separate from drawing

**Read:** `src/eye/EyeTypes.h`, then the overview comment in
`src/eye/EyeEngine.h`.

The engine decides *what* the eyes do and outputs a `FaceState` in degrees
and 0..1 amounts. The renderer turns that into pixels. Neither knows about
Arduino, which is why both also compile on a PC.

**Exercise:** build the frame generator and watch it in the page's live
mirror:

```sh
F=firmware/MochiEyes
g++ -std=c++17 -O2 -I$F tools/preview/frames.cpp $F/src/eye/EyeEngine.cpp $F/src/eye/EyeRenderer.cpp -o build/frames
./build/frames build/frames.bin 20 30
python tools/preview/serve.py      # http://127.0.0.1:8000
```

Then change the `setMood` calls in `frames.cpp`, rebuild, and compare.

## Step 3: A state machine you can test

**Read:** `src/touch/TouchGestures.h` (the timeline comment first), then
`tests/host/test_touch.cpp`.

Telling one tap from two needs memory of the past: a *state machine*. The
recognizer is fed `(level, time)` and returns events. Because it never calls
`millis()` itself, a test can feed it any timeline it likes.

**Exercise:** add a test for a "triple tap with a slow last tap" (the third
press comes 400 ms after the second). Predict the result before you run it.

## Step 4: The web page and its API

**Read:** `docs/API.md`, then the `/* ---- talking to Mochi ---- */` and
`/* ---- state ---- */` sections of `web/index.html`.

The page is static. Everything live comes from `GET /api/state` every 2
seconds and `GET /api/frame` about 6 times a second.

**Exercise:** run `python tools/preview/serve.py`, open
`http://127.0.0.1:8000`, and add a row to the Status panel showing
`sys.fetchStackFree`. Then run `python tools/embed_web.py` to embed your
change.

## Step 5: Reading the sketch top to bottom

**Read:** `MochiEyes.ino`. The banner comments divide it into sections in
dependency order: types, globals, time, sound, display, storage, network,
reactions, clock pages, drawing, overlays, modes, touch, commands, web
server, setup and loop.

Things to notice:

- `loop()` never waits. Long jobs are split into small steps driven by time
  stamps (look at `serviceSound()` and the melody sequencer).
- Comparisons like `(int32_t)(now - deadline) >= 0` stay correct when
  `millis()` wraps after 49 days.
- `restoreBase()` and `startReaction()` show how a temporary behaviour is
  layered on top of a user's choice and then removed.

**Exercise:** add a "wink" for 5 taps. Hint: copy the `Dizzy` approach in
`drawEyes()` and set `fs.eye[0].blink = 1` for one eye only.

## Step 6: Two tasks on one core

**Read:** `docs/adr/0003-network-fetches-in-a-freertos-task.md`, then
`fetchTask()` and `struct DataLock` in the sketch.

An HTTPS request can take seconds. Running it in a second task keeps the
eyes moving. The cost is that both tasks touch `wx` and `coins`, so access is
wrapped in a mutex. `DataLock` takes it in its constructor and releases it in
its destructor (RAII), so an early `return` cannot leave it locked.

**Exercise:** on hardware, open the Status panel and watch
`sys.fetchStackFree` after a refresh. Is 12 KB of stack too much? Change it,
measure again, and write down your result.

## Step 7: Fitting in flash

**Read:** `docs/adr/0004-minimal-spiffs-partition.md` and the Evaluation
section of the README.

**Exercise:** compile with `--warnings all` and note the "Sketch uses" line.
Comment out the crypto page's `fetchCrypto` call and compile again. How much
did HTTPS for one API cost? (The TLS library stays linked for open-meteo, so
expect less than you might guess.)

## Where to go next

- Add over-the-air updates (`ArduinoOTA`): the partition already has two app
  slots.
- Read `docs/RESEARCH.md` in the upstream eye engine project for the human
  eye-movement studies behind every number in `BehaviorParams.h`.
- Pick an item from the README Roadmap and open a pull request.
