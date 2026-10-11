#!/usr/bin/env python3
"""Preview the controller page on a PC, without a device.

Serves web/index.html and imitates the firmware's JSON API (same routes and
fields as MochiEyes.ino), so you can work on the page with a fast edit-reload
loop. The live mirror plays frames rendered by the real eye engine
(tools/preview/frames.cpp); without them it shows a still test pattern.

Usage (from the repository root):
    python tools/preview/serve.py [--port 8000] [--frames build/frames.bin]
Then open http://127.0.0.1:8000
"""
import argparse
import json
import pathlib
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

ROOT = pathlib.Path(__file__).resolve().parents[2]
FRAME = 1024

STATE = {
    "owner": "Kitty", "mode": "eyes", "screen": "clock",
    "eye": {"mood": "calm", "expr": "neutral", "state": "eye contact", "react": "none",
            "sleeping": False, "drowsy": False, "demo": False},
    "time": {"synced": True, "hhmm": "", "date": ""},
    "wifi": {"ap": False, "ssid": "Home 2.4G", "ip": "192.168.1.42", "rssi": -58, "up": True, "host": "mochi.local"},
    "oled": {"ok": True, "sda": 20, "scl": 21, "addr": 60},
    "weather": {"ok": True, "city": "Bengaluru", "temp": 24.6, "unit": "C", "word": "Part cloudy", "hum": 71, "min": 20, "max": 29},
    "coins": [{"tag": "BTC", "ok": True, "usd": "64210", "chg": 1.4},
              {"tag": "ETH", "ok": True, "usd": "3120", "chg": -0.8},
              {"tag": "SOL", "ok": True, "usd": "148.22", "chg": 3.1}],
    "fetching": False,
    "note": {"title": "Notes", "text": "Tap the pad to change pages.\n\nOpen http://mochi.local\nto write here."},
    "settings": {"sound": True, "autosleep": True, "start": "eyes"},
    "sys": {"uptime": 0, "heap": 171000, "minHeap": 148000, "fps": 35.2, "fetchStackFree": 5200},
}
STARTED = time.time()
FRAMES: list = []


def test_pattern() -> bytes:
    fb = bytearray(FRAME)
    for x in range(128):
        for y in (0, 63):
            fb[(y >> 3) * 128 + x] |= 1 << (y & 7)
    return bytes(fb)


def run_command(cmd: str) -> tuple:
    parts = cmd.split()
    if not parts:
        return False, "?"
    verb, arg = parts[0].lower(), (parts[1] if len(parts) > 1 else "")
    e = STATE["eye"]
    if verb == "mood":
        e["mood"] = arg
    elif verb == "expr":
        e["expr"] = arg
    elif verb == "sleep":
        e["sleeping"] = True
    elif verb == "wake":
        e["sleeping"] = False
    elif verb == "demo":
        e["demo"] = arg != "off"
    elif verb == "gesture":
        e["react"] = {"1": "glance", "2": "happy", "3": "grumpy", "4": "dizzy", "pet": "petting"}.get(arg, "none")
        STATE["mode"] = "eyes"
    elif verb == "lookn":
        e["state"] = "manual"
    elif verb == "auto":
        e["state"] = "eye contact"
    return True, "ok " + cmd


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, body, ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _json(self, obj, code=200):
        self._send(code, json.dumps(obj).encode())

    def log_message(self, fmt, *args):  # keep the console quiet
        if not self.path.startswith(("/api/frame", "/api/state")):
            super().log_message(fmt, *args)

    def do_GET(self):
        if self.path == "/":
            self._send(200, (ROOT / "web" / "index.html").read_bytes(), "text/html; charset=utf-8")
        elif self.path.startswith("/api/state"):
            now = time.localtime()
            STATE["time"]["hhmm"] = time.strftime("%H:%M", now)
            STATE["time"]["date"] = time.strftime("%a %d %b", now)
            STATE["sys"]["uptime"] = int(time.time() - STARTED) + 3 * 3600
            self._json(STATE)
        elif self.path.startswith("/api/frame"):
            i = int((time.time() - STARTED) * 20) % len(FRAMES)
            self._send(200, FRAMES[i], "application/octet-stream")
        else:
            self._json({"ok": False, "error": "unknown endpoint"}, 404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        form = {k: v[0] for k, v in parse_qs(self.rfile.read(n).decode()).items()}
        route = self.path
        if route == "/api/eye":
            ok, reply = run_command(form.get("cmd", ""))
            return self._json({"ok": ok, "reply": reply}, 200 if ok else 400)
        if route == "/api/mode":
            m = form.get("m", "toggle")
            STATE["mode"] = m if m in ("eyes", "clock") else ("clock" if STATE["mode"] == "eyes" else "eyes")
        elif route == "/api/screen":
            STATE["screen"], STATE["mode"] = form.get("s", "clock"), "clock"
        elif route == "/api/note":
            STATE["note"] = {"title": form.get("title") or "Notes", "text": form.get("text", "")}
            STATE["screen"], STATE["mode"] = "notes", "clock"
        elif route == "/api/settings":
            s = STATE["settings"]
            for k in ("sound", "autosleep"):
                if k in form:
                    s[k] = form[k] == "1"
            if "start" in form:
                s["start"] = form["start"]
        elif route not in ("/api/refresh", "/api/wifi"):
            return self._json({"ok": False, "error": "unknown endpoint"}, 404)
        self._json({"ok": True, "reply": "ok"})


def main():
    ap = argparse.ArgumentParser(description="Preview the MochiEyes web page without a device.")
    ap.add_argument("--port", type=int, default=8000)
    ap.add_argument("--frames", default=str(ROOT / "build" / "frames.bin"))
    args = ap.parse_args()
    p = pathlib.Path(args.frames)
    if p.exists():
        data = p.read_bytes()
        FRAMES.extend(data[i:i + FRAME] for i in range(0, len(data) - FRAME + 1, FRAME))
    if not FRAMES:
        print(f"no frames at {p}; showing a test pattern (see tools/preview/frames.cpp)")
        FRAMES.append(test_pattern())
    print(f"MochiEyes preview on http://127.0.0.1:{args.port}  ({len(FRAMES)} frames)")
    ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
