# ADR 0002: One display library, one shared frame buffer

- Status: Accepted
- Date: 2026-10-11

## Context

The eyes-only sketch used U8g2 by default (Adafruit SSD1306 was a compile-time
option). The clock used Adafruit SSD1306 + GFX for its text and icons. Linking
both libraries would cost flash we do not have
([ADR 0004](0004-minimal-spiffs-partition.md)), and two drivers would fight
over the same I2C panel.

The eye renderer does not use any library drawing calls. It writes pixels
straight into a 1024-byte buffer in the SSD1306 page layout
(`(y / 8) * 128 + x`, bit `y % 8`). Adafruit SSD1306 keeps exactly that layout
in `getBuffer()`.

## Decision

Use Adafruit SSD1306 only. The eyes render into `display.getBuffer()`; the
clock draws into the same buffer with GFX calls; overlays (hold bar, shoji
doors, captions) are drawn on top with GFX; one `display.display()` sends the
result.

A small subclass, `Oled`, allocates the buffer itself (`allocBuffer()`), since
Adafruit only allocates it inside `begin()`, which we call only when a panel
answers on I2C. Drawing therefore always has a buffer, even with no OLED
attached.

## Consequences

- One driver, one I2C owner, less flash.
- Text and shapes can be layered over the eyes for free.
- With no OLED connected, the firmware still renders every frame, so the web
  page's live mirror (`/api/frame` returns this buffer) works for development
  without a screen.
- `Oled` relies on `Adafruit_SSD1306::buffer` being `protected`. This is true
  in 2.5.17; a library update that changes it would break the build loudly,
  not silently.
- Panel rotations other than 0 and 180 degrees are not supported, because the
  renderer assumes the native buffer layout.

## Alternatives considered

- **U8g2 for both.** Its buffer has the same layout, but porting the clock's
  GFX drawing (fonts, `getTextBounds`, circles, triangles) is a larger change
  with more risk.
- **Render the eyes into a separate buffer and copy.** Costs 1 KB of RAM and a
  copy per frame for no benefit.
