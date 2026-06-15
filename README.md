# GeekBar Pulse X Screen Reuse

![PXL_20251016_232409591](https://github.com/user-attachments/assets/f5fbc51c-dfc7-4388-912e-b49b4bb5c5c0)

This repo aims to divert old GeekBar Pulse X and other disposable vapes from the landfill by reusing the SPI LED displays from the devices. There are 3 *.ino files available: general_use.ino (the general testing/display mapping script), CompleteDisplayMap.ino, which is the same as general_use.ino but with more features (thanks, masterPlusTer), and tempsense.ino (a demo temperature sensor script for use with an AGT20 or AGT10 sensor.) The scripts were tested on an Arduino UNO, but they probably work on most other microcontrollers.

The [`pulsex_jam/`](pulsex_jam/) folder adds support for the **Pulse X Jam** (which uses the same wire protocol but a completely different physical-to-bit mapping), and includes notes for running on the **ESP32-C3 Super Mini** in addition to UNO.

# IMPORTANT - READ BEFORE YOU TAKE THE VAPE APART!

For instructions on disassembling the GeekBar Pulse X disposable vape, I like this video: https://www.youtube.com/watch?v=1qDz5shnr1c&t=491s

Make sure to follow the instructions carefully, as there are lots of little things you could break inside the vape if you're not paying attention. 

# Screen compatibility:
Tempsense.ino only works with regular GeekBar Pulse X's, not any special editions like Pulse X edition or Pulse X Jam; you will have to change the mappings for it to work.
General_use.ino is compatible with all GeekBar Pulse X devices, and other vapes that use the same kind of display, like the Viho TRX 50K (not tested), with the exclusion of the "num", "digit", and "digits' serial commands, and the "displayDigit()", "displayDigits()", and "displayNumber()" functions. You will have to change the mappings for those to work.

For the **Pulse X Jam** specifically, the remapped sketch + full segment map is in [`pulsex_jam/`](pulsex_jam/).

# Serial command syntax:

baud: 9600

num N           -> Show number N (0-999999)

digit X Y       -> Show digit Y on position X (1-6)

digits a b c d e f -> Show 6 digits (use -1 for blank)

set X Y         -> Turn ON byte X bit Y

clr X Y         -> Turn OFF byte X bit Y

all 1           -> Turn all segments ON

all 0           -> Turn all segments OFF

clear           -> Clear display

# Function syntax:

setBit(int byteindex/segment, int bitindex/brightness, 0/1 for on/off)

clearDigits()

displayDigit(int digitindex, int value)

displayDigits(int d1, d2, d3, d4, d5, d6)

displayNumber(long num)

# Display mappings:

<img width="1536" height="1207" alt="pulsexmap" src="https://github.com/user-attachments/assets/ede244f0-5974-4609-8014-b0dea0dcc0de" />

1f: 7,7 1e: 15,7 2f: 47,7 2e: 55,7 3a: 0,7 3b: 1,7 3c: 2,7 3d: 3,7 3e: 4,7 3f: 5,7 3g: 6.7 4a: 8,7 4b: 9,7 4c: 10,7 4d: 11,7 4e: 12,7 4f: 13,7 4g: 14,7 5a: 40,7 5b: 41,7 5c: 42,7 5d: 43,7 5e: 44,7 5f: 45,7 5g: 46,7 6a: 48,7 6b: 49,7 6c: 50,7 6d: 51,7 6e: 52,7 6f: 53,7 6g: 54,7

# Pulse X Jam support

The **Pulse X Jam** uses the **same PLS916H wire protocol** as the regular Pulse X (1 MHz SPI mode 0, MSB-first, identical 13-byte header `5A FF 01 5A 24 21 3D 01 83 5A FF 02 5B`, 144-byte payload, 1-byte additive checksum, 4-byte tail `5A FF 04 5D`) — so the `write_pls916h()` function from the regular sketch works on the Jam unchanged. But the **physical-to-bit segment mapping is completely different**, so flashing the regular `general_use.ino` to a Jam will light segments without forming the right digits in the right places.

The [`pulsex_jam/`](pulsex_jam/) folder has a full remap:

- [`pulsex_jam/general_use.ino`](pulsex_jam/general_use.ino) — the regular sketch's protocol code with the Jam mapping baked in, plus area-aware helpers (`e N` / `j N` for energy and juice independently), label icons, jam icon parts, and a small star animation runner.
- [`pulsex_jam/jam_map.txt`](pulsex_jam/jam_map.txt) — every one of the 144 bytes documented (no bits wasted on the Jam — all 144 drive something visible).
- [`pulsex_jam/README.md`](pulsex_jam/README.md) — wiring table, command list, and ESP32-C3 setup notes.

## Jam byte map at a glance

| Bytes | What it controls |
| --- | --- |
| 0-6 | Energy area: tens digit, segments a-g |
| 7, 15 | Energy area: small "1" prefix (b = top of vertical line, c = bottom) |
| 8-14 | Energy area: ones digit, a-g |
| 16-22 | Juice area: tens digit, a-g |
| 23, 31 | Juice area: small "1" prefix |
| 24-30 | Juice area: ones digit, a-g |
| 32 | "ENERGY" label icon |
| 33-39 | Jam icon: top bar (7 LEDs) |
| 40-44 | Jam icon: bottom bar (5 LEDs) |
| 45-48 | Jam icon: center (JAM letters / diamond) |
| 49 | "JUICE" label icon |
| 50-104 | Decorative stars / comets / splatters (left wrap of the LCD) |
| 105-143 | Decorative stars / comets / splatters (right wrap) |

So the Jam can display two independent 0-199 numbers (each with the auto-handled "1" prefix), toggle the two label icons, toggle the central JAM icon in parts or whole, and animate the decorative stars.

## Jam-specific serial commands

In addition to all the standard commands from above (`num`, `digit`, `digits`, `set`, `clr`, `all 1`, `all 0`, `clear`), the Jam sketch adds:

```
e N               -> ENERGY area = N (0-199, with "1" prefix at >=100)
j N               -> JUICE area = N (0-199)
100               -> shows 100 in both areas
energy on|off     -> "ENERGY" label icon
juice on|off      -> "JUICE" label icon
jam top|bot|center|full|off
stars on|off|left|right
anim off|sweep|trail|sparkle
animspeed N       -> ms per animation step (default 80)
traillen N        -> length of the comet tail in trail mode
```

## ESP32-C3 host support

The Jam sketch also runs on an **ESP32-C3 Super Mini** (and other Arduino-ESP32 boards). It's the same code — only the SPI setup is conditional. Three things to be aware of on the C3:

1. **JTAG on GPIO 4-7.** The Espressif USB-Serial-JTAG controller holds GPIO 4-7 unless explicitly detached. The sketch calls `gpio_reset_pin()` on the SCK and MOSI pins before `SPI.begin()` so SPI can drive them.
2. **Custom SPI pins.** On the C3, FSPI can be routed to any GPIO via the GPIO matrix. The sketch uses `SPI.begin(SCK, MISO, MOSI, SS)` with MISO and SS = -1 (the screen is write-only).
3. **USB CDC.** With Arduino-ESP32 core 3.x, set **Tools > USB CDC On Boot: Enabled** (FQBN option `CDCOnBoot=cdc`). Without it `Serial` goes to UART0 hardware pins instead of the USB port and you won't see any output.

Pin assignment used by the sketch:

| Screen pad | UNO  | ESP32-C3 |
| ---------- | ---- | -------- |
| V          | 3.3V | 3.3V     |
| G          | GND  | GND      |
| C          | 13   | GPIO 4   |
| D          | 11   | GPIO 6   |

# Wiring

Your GeekBar's display will look like this:
![vape-lcd-activation-v0-0mjdpd401a6e1](https://github.com/user-attachments/assets/37f14938-234e-4121-a5ae-5f791790619f)

The GND, VIN, CLK, and DIN pins may be labeled as G, V, C, and D on certain display revisions, but they are the same pins. These pins are what we are going to be using to interface with the screen. Connect V or VIN to 3.3v on your microcontroller, G or GND to ground on your microcontroller, and CLK/C and DIN/D to your microcontroller's MOSI (data) and SCK (clock) pins. For Arduino UNO, those pins are 13 for SCK and 11 for MOSI.
Note: On some screen revisions, the V or VIN pad is not connected to anything. In that case, you can connect 3.3v to C2 instead.

# Conclusion

After you have wired your display up, you can flash one of the sketches to your microcontroller using Arduino IDE. If you would like to make your own projects, you can use general_use.ino. Feel free to post the projects that you make with these usefull little displays in the discussions tab or on the subreddit r/streetlithium. If you have any questions, feel free to e-mail me at sawyermatheson037@gmail.com or PM u/Progressbar95 on Reddit.

WARNING: If you are allergic to AI generated code, please do not download anything from this repo. The code was written mostly using ChatGPT.
