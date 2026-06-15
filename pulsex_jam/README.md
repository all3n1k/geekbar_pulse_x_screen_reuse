# Pulse X Jam screen support

The GeekBar Pulse X **Jam** uses the same PLS916H wire protocol as the regular
Pulse X (1 MHz SPI mode 0, MSB-first, identical 13-byte header and 4-byte tail,
identical 144-byte payload + 1-byte checksum frame format), but the **physical-
to-bit mapping** is completely different — the segments and icons live at
different byte indices than in `../pulsex/general_use.ino`. So the regular sketch
will light segments on the Jam, but won't form the right digits in the right
places.

This folder is the Jam-specific version. The wire protocol code is identical to
the regular Pulse X sketch.

## What was mapped

All 144 bytes (bit 7) on the Jam are accounted for — see [`jam_map.txt`](jam_map.txt).
At a glance:

| Bytes      | What it controls                                                         |
| ---------- | ------------------------------------------------------------------------ |
| 0-6        | Energy area: tens digit, segments a-g                                    |
| 7, 15      | Energy area: small "1" prefix (b = top, c = bottom of the vertical line) |
| 8-14       | Energy area: ones digit, segments a-g                                    |
| 16-22      | Juice area: tens digit, a-g                                              |
| 23, 31     | Juice area: small "1" prefix                                             |
| 24-30      | Juice area: ones digit, a-g                                              |
| 32         | "ENERGY" label icon                                                      |
| 33-39      | Central jam icon: top bar LEDs (7)                                       |
| 40-44      | Central jam icon: bottom bar LEDs (5)                                    |
| 45-48      | Central jam icon: the JAM letters / diamond LEDs                         |
| 49         | "JUICE" label icon                                                       |
| 50-104     | Decorative stars / comets / splatters wrapping the **left** side         |
| 105-143    | Decorative stars / comets / splatters wrapping the **right** side        |

This means the Jam can display **two independent numbers 0-199** (energy and
juice), each with the optional "1" prefix and the surrounding decorative state
turned on or off — no other bytes are wasted.

## Serial commands

Standard commands compatible with `../pulsex/general_use.ino`:
`num N`, `digit X Y`, `digits a b c d e f`, `set X Y`, `clr X Y`, `all 1`, `all 0`, `clear`.

Jam-specific helpers:

```
e N               -> show 0-199 in the ENERGY area  (auto handles 1-prefix at >=100)
j N               -> show 0-199 in the JUICE area
100               -> 100 on both
energy on|off     -> "ENERGY" label icon
juice on|off      -> "JUICE" label icon
jam top|bot|center|full|off
stars on|off|left|right
anim off|sweep|trail|sparkle
animspeed N       -> ms per animation step (default 80)
traillen N        -> length of comet tail in trail mode (default 8)
```

## Wiring

Same as the regular Pulse X.

| Screen pad | Microcontroller pin                  |
| ---------- | ------------------------------------ |
| V          | 3.3 V                                |
| G          | GND                                  |
| C          | SCK   (UNO: pin 13, ESP32-C3: GPIO 4)|
| D          | MOSI  (UNO: pin 11, ESP32-C3: GPIO 6)|

The screen daughterboard's J1 ribbon to the factory vape main board must be
disconnected — otherwise the factory chip and your microcontroller will fight
on the C and D lines.

## ESP32-C3 notes

This sketch also runs on an ESP32-C3 Super Mini (and similar boards) when built
with the Arduino-ESP32 core. Three things matter:

1. **JTAG conflict.** GPIO 4-7 on the C3 are also the JTAG pins
   (MTMS/MTDI/MTCK/MTDO) and the USB-Serial-JTAG controller will hold them
   unless you explicitly detach them. The sketch calls `gpio_reset_pin()` on
   GPIO 4 and 6 before `SPI.begin()` to handle this.

2. **Custom SPI pins.** On the C3, FSPI can be routed to any GPIO via the GPIO
   matrix. The sketch uses `SPI.begin(SCK, MISO, MOSI, SS)` with MISO and SS
   set to -1 since the screen is write-only.

3. **USB CDC.** In Arduino-ESP32 core 3.x, set **Tools > USB CDC On Boot:
   Enabled** (FQBN option `CDCOnBoot=cdc`). Without it `Serial` writes go to
   UART0 hardware pins instead of the USB port.

A working build line for arduino-cli:

```
arduino-cli compile -b esp32:esp32:esp32c3:CDCOnBoot=cdc general_use.ino
arduino-cli upload  -b esp32:esp32:esp32c3:CDCOnBoot=cdc -p /dev/cu.usbmodem101 general_use.ino
```

The whole driver is platform-agnostic except for the small `#if defined(ARDUINO_ARCH_ESP32)` block
in `setup()`. On UNO it just calls `SPI.begin()` and everything else is the same.
