# ADR 0006: The web page is authored as HTML and embedded gzipped

- Status: Accepted
- Date: 2026-10-11

## Context

`clock.ino` built its page at request time by concatenating `String`s in
C++, which is hard to design, impossible to preview in a browser, and allocates
heap on every request. The redesigned controller is a full page (CSS, SVG art,
JavaScript for the gaze pad and live mirror) of about 39 KB.

## Decision

- The page lives in `web/index.html` as a normal file that opens in any
  browser.
- `tools/embed_web.py` gzips it (level 9, `mtime=0` so the output is
  reproducible) into a byte array in `src/web/WebUi.h`.
- The firmware serves those bytes from flash with
  `Content-Encoding: gzip`; browsers unpack them transparently. Dynamic data
  comes from a JSON API (`/api/state`) instead of being baked into the HTML.
- CI runs `python tools/embed_web.py --check`, which decompresses the header
  and compares it with the source, so the two cannot drift apart.
- `tools/preview/serve.py` serves the page with a mock API, so the design can
  be iterated on a PC with real eye frames in the mirror.

## Consequences

- Measured: 39,339 bytes of HTML become 13,238 bytes in flash (34%).
- Serving the page allocates no heap: `send_P` streams it from flash.
- Fonts (Shippori Mincho B1, Zen Kaku Gothic New) load from Google Fonts when
  the phone has internet. In hotspot mode they cannot load, and the page falls
  back to system Mincho and Gothic fonts. Layout does not depend on them.
- Contributors must remember to run the embed script; CI catches it if they
  forget.

## Alternatives considered

- **SPIFFS/LittleFS file.** Requires a separate upload step and a filesystem
  partition we shrank ([ADR 0004](0004-minimal-spiffs-partition.md)).
- **Raw (uncompressed) string literal.** Simple and editable in place, but
  three times the flash and slower to load over Wi-Fi.
