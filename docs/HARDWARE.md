# Hardware

## Parts

| Part | Notes |
| ---- | ----- |
| ESP32-C3 SuperMini | Any ESP32-C3 board with 4 MB flash works; pin numbers below are GPIO numbers |
| SSD1306 0.96" OLED, 128x64, I2C | Address 0x3C (0x3D is also tried). For 1.3" SH1106 panels see the notes at the end |
| Capacitive touch module (TTP223 or similar) | Output goes HIGH while touched |
| Passive buzzer | Driven with `tone()`; an active buzzer would only click |

## Wiring

The pin map comes from the original `clock.ino`, where each pin was confirmed
on the real board (the OLED with an I2C scanner). The eyes-only sketch used
the same OLED pins, so merging needed no rewiring.

| Signal        | ESP32-C3 GPIO | Connects to |
| ------------- | ------------- | ----------- |
| I2C SDA       | 20            | OLED SDA |
| I2C SCL       | 21            | OLED SCL |
| Touch         | 1             | Touch module OUT (I/O) |
| Buzzer        | 2             | Buzzer + (other leg to GND) |
| 3V3           | 3V3           | OLED VCC, touch VCC |
| GND           | GND           | OLED GND, touch GND, buzzer - |

```
 ESP32-C3 SuperMini            SSD1306 OLED
 ┌───────────────┐            ┌───────────┐
 │ GPIO20 ───────┼────────────┤ SDA       │
 │ GPIO21 ───────┼────────────┤ SCL       │
 │ 3V3 ──────────┼──────┬─────┤ VCC       │
 │ GND ──────────┼───┬──┼─────┤ GND       │
 │               │   │  │     └───────────┘
 │ GPIO1 ────────┼───┼──┼─────┐ TTP223
 │               │   │  └─────┤ VCC
 │               │   ├────────┤ GND
 │               │   │        └ OUT
 │ GPIO2 ────────┼───┼────────┐ Buzzer +
 │               │   └────────┘ Buzzer -
 └───────────────┘
```

Mount the touch pad on the **left** side as you face the screen: a single tap
makes the eyes glance towards it. If yours is on the right, set
`TOUCH_SIDE` to `+1` in `OvoEyes.ino`.

## Pin notes

- **GPIO20/21 are UART0.** They are free for I2C only when *USB CDC On Boot*
  is **Enabled**, which moves `Serial` to the native USB port. With it
  disabled, the firmware detects the clash and stays silent on Serial rather
  than break the display (`SERIAL_SAFE` in the sketch).
- **GPIO2 is a strapping pin.** It is read at power-on to choose the boot
  mode. A buzzer wired directly between GPIO2 and GND has a low DC resistance
  and can hold the pin low; if your board does not boot with the buzzer
  attached, drive the buzzer through a small transistor or move it to another
  free pin and change `PIN_BUZZER`.
- **GPIO8 and GPIO9** (onboard LED and BOOT button on the SuperMini) are also
  strapping pins. They are only touched by the OLED auto-search, which runs
  only if nothing answers on GPIO20/21.
- **I2C auto-search.** If the OLED is not found on the configured pins, the
  firmware tries common pin pairs and then every pair, skipping the touch and
  buzzer pins. The pins it used are shown in the web page's Status section.

## Boot signals

You get feedback even with a dead screen:

| Sound | Meaning |
| ----- | ------- |
| 1 short beep | Powered and running |
| 2nd short beep | OLED found |
| Long low beep | No OLED found (Wi-Fi and the web page still work, including the live mirror) |
| High beep | Joined Wi-Fi |

The Serial Monitor (115200 baud) prints an `alive` line every 10 seconds.

## Using a 1.3" SH1106 panel

The firmware uses the Adafruit SSD1306 driver. SH1106 panels have a
132-column RAM and show a 2-pixel offset with this driver. Supporting them
means swapping in an SH1106 driver that exposes a 1024-byte page-layout buffer;
see [ADR 0002](adr/0002-one-display-library-shared-frame-buffer.md) for why the
buffer layout matters.
