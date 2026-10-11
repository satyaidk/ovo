# Contributing to MochiEyes

Thanks for your interest. Bug reports, new reactions, clock pages and
documentation fixes are all welcome. This guide covers how to build, test and
submit a change.

## Ground rules

- Be respectful; see the [code of conduct](CODE_OF_CONDUCT.md).
- Open an issue before a large change so we can agree on the approach.
- Security problems go through [SECURITY.md](SECURITY.md), not public issues.

## Development setup

You need:

- Arduino IDE 2.x or `arduino-cli`, with the ESP32 core **3.3.12**
  (Boards Manager URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`)
- Libraries: Adafruit SSD1306 2.5.17, Adafruit GFX Library 1.12.6, ArduinoJson 7.4.3
- Python 3.8+ (for `tools/`)
- A C++17 compiler for host tests (g++, clang, or Docker with the `gcc:14` image)

In `firmware/MochiEyes/`, copy `secrets.example.h` to `secrets.h` and fill in
your Wi-Fi details. Everything that is flashed lives in that folder.

Compile from the command line exactly as CI does:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=min_spiffs --warnings all firmware/MochiEyes
```

The sketch folder must stay called `MochiEyes` (Arduino requires the folder
and the `.ino` to share a name).

In the table below, paths starting with `src/` or `MochiEyes.ino` are inside
`firmware/MochiEyes/`.

## Where to make a change

| You want to...                         | Edit                                   |
| -------------------------------------- | -------------------------------------- |
| Add or change a touch reaction         | `startReaction()` / `onEyesGesture()` in `MochiEyes.ino` |
| Change tap or hold timings             | `touch::Config` in `src/touch/TouchGestures.h`, then update the tests |
| Change how the eyes move or blink      | `src/eye/EyeEngine.cpp`, `src/eye/BehaviorParams.h` |
| Change how the eyes look               | `src/eye/EyeGeometry.h`                |
| Add a clock page                       | `Screen` enum and `drawClockMode()` in `MochiEyes.ino` |
| Change the web page                    | `web/index.html`, then run `python tools/embed_web.py` |
| Add a web API route                    | `startWebServer()` in `MochiEyes.ino`, `tools/preview/serve.py`, and [docs/API.md](docs/API.md) |

Never edit `src/web/WebUi.h` by hand; it is generated.

## Testing

Run the host tests before every pull request:

```sh
cd tests/host
g++ -std=c++11 -Wall -Wextra -Werror -I../../firmware/MochiEyes test_touch.cpp -o test_touch && ./test_touch
```

To work on the web page without a device:

```sh
F=firmware/MochiEyes
g++ -std=c++17 -O2 -I$F tools/preview/frames.cpp $F/src/eye/EyeEngine.cpp $F/src/eye/EyeRenderer.cpp -o build/frames
./build/frames build/frames.bin 20 30
python tools/preview/serve.py      # http://127.0.0.1:8000
```

Then try the change on real hardware if you can, and say in the pull request
which board, OLED and touch module you used.

## Style

- Match the surrounding code: 2-space indent, `camelCase` functions,
  comments that explain *why*, not *what*.
- Keep `src/eye/` and `src/touch/` free of Arduino headers so they stay
  testable on a PC.
- Nothing in `loop()` may block for more than a few milliseconds; network
  work belongs in `fetchTask` (see [ADR 0003](docs/adr/0003-network-fetches-in-a-freertos-task.md)).
- Watch flash use: paste the `Sketch uses ... bytes` line in your pull request
  if it changes by more than a few kilobytes.

## Commits and pull requests

- One logical change per pull request.
- Write commit messages in the imperative mood: "Add wink reaction", not
  "Added wink reaction".
- Update [CHANGELOG.md](CHANGELOG.md) under **Unreleased**.
- Fill in the pull request template, including how you tested.

## Recording a design decision

If your change makes a choice future contributors will wonder about (a new
library, a different partition, a protocol change), add an ADR under
`docs/adr/` using the existing files as a template, and link it from
[docs/DESIGN.md](docs/DESIGN.md).
