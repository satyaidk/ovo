# Web API

The firmware serves a small JSON API on port 80. The controller page uses it,
and so can scripts or home-automation tools on the same network.

- Base URL: `http://mochi.local` (or the IP shown on the OLED at boot;
  `http://192.168.4.1` in hotspot mode).
- `POST` bodies are form-encoded (`application/x-www-form-urlencoded`).
- Every `POST` reply is JSON: `{"ok": true, "reply": "..."}` on success, or
  `{"ok": false, "error": "..."}` with HTTP 400/404.
- There is no authentication. See [SECURITY.md](../SECURITY.md).

`GET` requests (`/api/state`, `/api/frame`) do not count as interaction, so an
open browser tab never keeps Mochi awake. `POST` requests do.

## Endpoints

| Method | Path            | Parameters                             | Effect |
| ------ | --------------- | -------------------------------------- | ------ |
| GET    | `/`             |                                        | The controller page (gzipped HTML) |
| GET    | `/api/state`    |                                        | Everything the page shows (below) |
| GET    | `/api/frame`    |                                        | The current OLED frame: 1024 raw bytes |
| POST   | `/api/eye`      | `cmd` = a command line (below)         | Runs one console command |
| POST   | `/api/mode`     | `m` = `eyes`, `clock` or `toggle`      | Switches the face |
| POST   | `/api/screen`   | `s` = `clock`, `weather`, `crypto`, `notes`, `next`, `prev` | Shows a clock page (switches to the clock face) |
| POST   | `/api/note`     | `title` (max 18), `text` (max 1400)    | Saves the note and shows it |
| POST   | `/api/settings` | any of `sound`=0/1, `autosleep`=0/1, `start`=`eyes`/`clock` | Saves settings to flash |
| POST   | `/api/refresh`  |                                        | Fetches weather and prices now |
| POST   | `/api/wifi`     | `ssid`, `pass`                         | Saves credentials and restarts after ~1.2 s |

## Commands for `/api/eye` (and the Serial Monitor)

The same command language works over Serial at 115200 baud.

App commands, handled in `MochiEyes.ino`:

| Command                   | Effect |
| ------------------------- | ------ |
| `mode eyes` / `mode clock` / `mode` | Switch face (no argument toggles) |
| `gesture 1`               | Glance toward the touch pad |
| `gesture 2`               | Smile |
| `gesture 3`               | Grumpy side-eye, then a glare |
| `gesture 4`               | Dizzy |
| `gesture pet`             | Petting for 2.5 s, then a smile |
| `demo` / `demo on` / `demo off` | The 103-second behaviour showcase |

Eye engine commands, handled by `src/eye/EyeCommands.cpp`:

| Command                   | Effect |
| ------------------------- | ------ |
| `look <yaw> <pitch>`      | Look at a direction in degrees (+yaw = screen right, +pitch = up) |
| `lookn <x> <y>`           | Same, normalised to -1..1 (the gaze pad uses this) |
| `auto`                    | Let the engine choose where to look |
| `mood <name>`             | `calm`, `listening`, `talking`, `curious`, `focused`, `sleepy` |
| `expr <name>`             | `neutral`, `happy`, `sad`, `angry`, `surprised`, `sleepy`, `suspicious` |
| `blink`                   | Blink now |
| `startle <yaw> <pitch>`   | Something happened over there |
| `sleep` / `wake`          | Close / open the eyes |
| `pause`                   | Phrase boundary while talking (likely blink) |
| `light <0..1>`, `arousal <0..1>`, `near <cm>`, `user <yaw> <pitch>` | Engine inputs |
| `style human` / `style mochi` | Switch the drawing preset |
| `feature <name> <0/1>`    | Toggle a mechanism: `saccades micro blinks lids pupil head vergence corrective gazeblink` |
| `status`, `help`          | Text replies |

`mood` and `expr` set Mochi's **base** personality: what it returns to after
a touch reaction or after waking up.

Example:

```sh
curl -d "cmd=mood curious" http://mochi.local/api/eye
curl -d "m=clock" http://mochi.local/api/mode
curl -d "title=Today" --data-urlencode "text=Buy milk" http://mochi.local/api/note
```

## `GET /api/state`

```jsonc
{
  "owner": "Kitty",
  "mode": "eyes",                 // "eyes" | "clock"
  "screen": "clock",              // current clock page
  "eye": {
    "mood": "calm", "expr": "neutral",   // base personality
    "state": "eye contact",       // engine attention: eye contact, glance away, explore, reading, manual, pursuit, sleep
    "react": "none",              // none, glance, happy, grumpy, dizzy, petting, waking
    "sleeping": false, "drowsy": false, "demo": false
  },
  "time": { "synced": true, "hhmm": "20:41", "date": "Sun 11 Oct" },
  "wifi": { "ap": false, "ssid": "Home", "ip": "192.168.1.42", "rssi": -58, "up": true, "host": "mochi.local" },
  "oled": { "ok": true, "sda": 20, "scl": 21, "addr": 60 },
  "weather": { "ok": true, "city": "Bengaluru", "temp": 24.6, "unit": "C", "word": "Part cloudy", "hum": 71, "min": 20, "max": 29 },
  "coins": [ { "tag": "BTC", "ok": true, "usd": "64210", "chg": 1.4 } ],
  "fetching": false,
  "note": { "title": "Notes", "text": "..." },
  "settings": { "sound": true, "autosleep": true, "start": "eyes" },
  "sys": { "uptime": 3600, "heap": 171000, "minHeap": 148000, "fps": 35.2, "fetchStackFree": 5200 }
}
```

The numbers above are illustrative. `sys.fetchStackFree` is the fetch task's
stack high-water mark in bytes: how much of its 12 KB stack has never been used.

## `GET /api/frame`

1024 bytes, the exact buffer sent to the SSD1306: byte index
`(y / 8) * 128 + x`, bit `y % 8`, 1 = lit. Decoding in JavaScript:

```js
const lit = (buf[(y >> 3) * 128 + x] >> (y & 7)) & 1;
```

The controller page polls this about 6 times per second while visible.
Each request briefly occupies the single-threaded web server, so polling
much faster lowers the eye frame rate.
