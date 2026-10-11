# ADR 0003: Network fetches run in a FreeRTOS task

- Status: Accepted
- Date: 2026-10-11

## Context

`clock.ino` fetched weather (every 15 minutes) and coin prices (every minute)
directly in `loop()` with blocking `HTTPClient` calls over HTTPS, with a
12-second timeout. On the clock face a pause while the page waited for data was
acceptable. On the eyes it is not: a TLS handshake on the ESP32-C3 takes on
the order of seconds, and during that time the eyes would freeze mid-saccade or
mid-blink, breaking the illusion that the device is alive.

## Decision

Move all HTTP requests into a dedicated FreeRTOS task, `fetchTask`
(12 KB stack, priority 1, same as the Arduino loop task).

- The task sleeps on `ulTaskNotifyTake()` with a 5-second timeout, wakes,
  and fetches whatever is due. `requestFetch(force)` wakes it early (long
  press, web Refresh button, switching to the clock face).
- Results are built in local variables and copied into the shared `wx` and
  `coins` while holding a FreeRTOS mutex (`dataLock`). Readers in the loop task
  (drawing, `/api/state`) take the same mutex through the `DataLock` helper.
- A failed request is retried after 30 seconds rather than on every wake, so
  an internet outage does not hammer the APIs.
- Prices refresh every minute while the clock is shown, and only every 10
  minutes while the eyes are shown, since nobody is reading them.

The ESP32-C3 has one core. The two tasks share it by time-slicing, and the
fetch task spends most of a request blocked on the network, so the loop task
keeps rendering frames. `loop()` calls `delay(1)` when no frame is due, which
yields the core instead of busy-waiting.

## Consequences

- The eyes keep moving during downloads. The frame rate may dip while the TLS
  handshake does its maths, but there is no multi-second freeze.
- Shared state needs care: any new data written by the task must be guarded
  by `dataLock`. This is the main new concept for contributors, and it is
  explained at the top of the network section in the sketch.
- About 12 KB of heap goes to the task stack. Its real headroom is reported at
  runtime as `sys.fetchStackFree` in `/api/state`, so the size can be tuned
  from measurements rather than guesses.

## Alternatives considered

- **Async HTTP library.** Adds a dependency and flash, and HTTPS support in
  async clients for the C3 is less mature.
- **Fetch only while the clock is visible.** Simple, but entering the clock
  would show "Loading..." for seconds every time.
- **Lower timeouts.** Reduces the worst case, but a normal handshake would
  still freeze the eyes.
