// Pulse X JAM screen driver, in the same style as ../pulsex/general_use.ino.
//
// The Jam uses the same PLS916H wire protocol (1 MHz SPI mode 0, MSB-first, the
// same 13-byte header and 4-byte tail) but the physical-to-bit mapping for the
// digits and icons is different from the regular Pulse X. See jam_map.txt for
// the full segment map (all 144 bytes mapped).
//
// Tested on Arduino UNO and ESP32-C3 Super Mini.
//   UNO:  SCK = pin 13, MOSI = pin 11, V = 3.3 V
//   C3 :  SCK = GPIO 4, MOSI = GPIO 6, V = 3.3 V
//         (GPIO 4-7 on C3 are also JTAG pins; gpio_reset_pin() detaches them
//          so SPI can drive them. Also requires "USB CDC On Boot: Enabled".)
//
// Serial commands (baud 9600):
//   Compatible with general_use.ino:
//     num N            -> show number on the four big digits (e.g. num 4250)
//     digit X Y        -> show digit Y on position X (1=energy small "1",
//                         2=juice small "1", 3=energy tens, 4=energy ones,
//                         5=juice tens, 6=juice ones)
//     digits a b c d e f
//     set X Y / clr X Y
//     all 1 / all 0 / clear
//   Jam-specific:
//     e N              -> show 0-199 in the ENERGY area (auto handles "1" prefix)
//     j N              -> show 0-199 in the JUICE area
//     100              -> show "100" on both
//     energy on|off    -> ENERGY label icon
//     juice  on|off    -> JUICE label icon
//     jam top|bot|center|full|off
//     stars on|off|left|right
//     anim off|sweep|trail|sparkle      (animates the decorative stars)
//     animspeed N      -> ms per animation step (default 80)
//     traillen N       -> length of the comet tail in 'trail' mode

#include <Arduino.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_ESP32)
  #include "driver/gpio.h"
  #define ESP32_SCK_PIN  4
  #define ESP32_MOSI_PIN 6
#endif

// ---------- PLS916H wire protocol (shared with regular Pulse X) ----------

struct PLS916H_Packet {
  uint8_t header[13];
  uint8_t data[144];
  uint8_t checksum;
  uint8_t tail[4];
} __attribute__((packed));

uint8_t frame[144];

uint8_t chk8_pls916h(uint8_t data[144]) {
  uint8_t b = 0;
  for (int i = 0; i < 144; i++) b += data[i];
  return b;
}

void write_pls916h(uint8_t data[144]) {
  static PLS916H_Packet pkt = {
    {0x5A, 0xFF, 0x01, 0x5A, 0x24, 0x21, 0x3D, 0x01, 0x83, 0x5A, 0xFF, 0x02, 0x5B},
    {0}, 0,
    {0x5A, 0xFF, 0x04, 0x5D}
  };
  memcpy(pkt.data, data, 144);
  pkt.checksum = chk8_pls916h(data);

  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  uint8_t *raw = (uint8_t *)&pkt;
  for (size_t i = 0; i < sizeof(pkt); i++) SPI.transfer(raw[i]);
  SPI.endTransaction();

  SPI.beginTransaction(SPISettings(200000, MSBFIRST, SPI_MODE0));
  SPI.transfer(0);
  SPI.endTransaction();
}

void setBit(int byteIndex, int bitIndex, bool on) {
  if (byteIndex < 0 || byteIndex >= 144) return;
  if (on) frame[byteIndex] |= (1 << bitIndex);
  else    frame[byteIndex] &= ~(1 << bitIndex);
}

void clearDigits() { memset(frame, 0, sizeof(frame)); }

// ---------- Jam-specific segment map ----------

struct Segment { int byteIndex; int bitIndex; };

// digit positions:
//   1 = energy small "1" prefix (only b + c segments, forms a vertical line)
//   2 = juice  small "1" prefix
//   3 = energy tens (big),  4 = energy ones (big)
//   5 = juice  tens (big),  6 = juice  ones (big)
Segment digitSegments[6][7] = {
  {{-1,-1},{ 7,7},{15,7},{-1,-1},{-1,-1},{-1,-1},{-1,-1}}, // 1: energy "1"  b=byte7, c=byte15
  {{-1,-1},{23,7},{31,7},{-1,-1},{-1,-1},{-1,-1},{-1,-1}}, // 2: juice  "1"  b=byte23, c=byte31
  {{ 0,7},{ 1,7},{ 2,7},{ 3,7},{ 4,7},{ 5,7},{ 6,7}},      // 3: energy tens (a..g)
  {{ 8,7},{ 9,7},{10,7},{11,7},{12,7},{13,7},{14,7}},      // 4: energy ones
  {{16,7},{17,7},{18,7},{19,7},{20,7},{21,7},{22,7}},      // 5: juice  tens
  {{24,7},{25,7},{26,7},{27,7},{28,7},{29,7},{30,7}}       // 6: juice  ones
};

const bool numberTable[10][7] = {
  {1,1,1,1,1,1,0}, // 0
  {0,1,1,0,0,0,0}, // 1
  {1,1,0,1,1,0,1}, // 2
  {1,1,1,1,0,0,1}, // 3
  {0,1,1,0,0,1,1}, // 4
  {1,0,1,1,0,1,1}, // 5
  {1,0,1,1,1,1,1}, // 6
  {1,1,1,0,0,0,0}, // 7
  {1,1,1,1,1,1,1}, // 8
  {1,1,1,1,0,1,1}  // 9
};

// Icon byte indices (all bit 7). See jam_map.txt for the full mapping.
#define JAM_ENERGY_ICON  32
#define JAM_JUICE_ICON   49
#define JAM_TOPBAR_FIRST 33
#define JAM_TOPBAR_LAST  39
#define JAM_BOTBAR_FIRST 40
#define JAM_BOTBAR_LAST  44
#define JAM_CENTER_FIRST 45
#define JAM_CENTER_LAST  48
#define JAM_STARS_FIRST  50
#define JAM_STARS_LAST  143
#define JAM_STARS_RIGHT 105   // index where right-side decorations begin

void displayDigit(int digitIndex, int value) {
  if (digitIndex < 0 || digitIndex > 5) return;
  for (int seg = 0; seg < 7; seg++) {
    Segment s = digitSegments[digitIndex][seg];
    if (s.byteIndex >= 0) setBit(s.byteIndex, s.bitIndex, false);
  }
  if (value < 0 || value > 9) return;
  for (int seg = 0; seg < 7; seg++) {
    if (numberTable[value][seg]) {
      Segment s = digitSegments[digitIndex][seg];
      if (s.byteIndex >= 0) setBit(s.byteIndex, s.bitIndex, true);
    }
  }
}

void displayDigits(int d1, int d2, int d3, int d4, int d5, int d6) {
  clearDigits();
  int values[6] = {d1, d2, d3, d4, d5, d6};
  for (int i = 0; i < 6; i++) {
    if (values[i] >= 0 && values[i] <= 9) displayDigit(i, values[i]);
  }
}

void displayNumber(long num) {
  // Big digit ladder: digit 3 = energy tens, 4 = energy ones,
  // 5 = juice tens, 6 = juice ones. Right-to-left fill across all 4 big digits.
  clearDigits();
  int order[4] = {5, 4, 3, 2};   // ones-of-juice -> tens-of-energy
  for (int i = 0; i < 4; i++) {
    int digit = num % 10;
    num /= 10;
    displayDigit(order[i], digit);
    if (num == 0) break;
  }
}

// Show 0-199 in one of the two areas (energy = idx 0, juice = idx 1).
void displayArea(int area, int n) {
  int small1 = area == 0 ? 0 : 1;
  int tens   = area == 0 ? 2 : 4;
  int ones   = area == 0 ? 3 : 5;
  if (n < 0 || n > 199) return;
  if (n >= 100) { displayDigit(small1, 1); n -= 100; } else displayDigit(small1, -1);
  displayDigit(tens, n / 10);
  displayDigit(ones, n % 10);
}

void fillRange(int from, int to, bool on) {
  for (int b = from; b <= to; b++) setBit(b, 7, on);
}

// ---------- Star animation ----------

enum AnimMode { ANIM_NONE, ANIM_SWEEP, ANIM_TRAIL, ANIM_SPARKLE };
AnimMode animMode = ANIM_NONE;
int animPos = 0;
int animTrailLen = 8;
unsigned long animStep = 80;
unsigned long animNext = 0;

void clearStars() { fillRange(JAM_STARS_FIRST, JAM_STARS_LAST, false); }

void tickAnim() {
  if (animMode == ANIM_NONE) return;
  if (millis() < animNext) return;
  animNext = millis() + animStep;
  int len = JAM_STARS_LAST - JAM_STARS_FIRST + 1;
  clearStars();
  if (animMode == ANIM_SWEEP) {
    setBit(JAM_STARS_FIRST + animPos, 7, true);
    animPos = (animPos + 1) % len;
  } else if (animMode == ANIM_TRAIL) {
    for (int i = 0; i < animTrailLen; i++) {
      int idx = (animPos - i + len) % len;
      setBit(JAM_STARS_FIRST + idx, 7, true);
    }
    animPos = (animPos + 1) % len;
  } else if (animMode == ANIM_SPARKLE) {
    for (int b = JAM_STARS_FIRST; b <= JAM_STARS_LAST; b++) {
      if ((random(8)) == 0) setBit(b, 7, true);  // ~12.5% on
    }
  }
}

// ---------- setup / loop ----------

void setup() {
#if defined(ARDUINO_ARCH_ESP32)
  // GPIO 4-7 on the ESP32-C3 are also the JTAG pins (MTMS/MTDI/MTCK/MTDO).
  // Detach them before SPI tries to drive them.
  gpio_reset_pin(GPIO_NUM_4);
  gpio_reset_pin(GPIO_NUM_6);
  SPI.begin(ESP32_SCK_PIN, -1, ESP32_MOSI_PIN, -1);
#else
  SPI.begin();
#endif
  Serial.begin(9600);
  memset(frame, 0, sizeof(frame));

  Serial.println("Pulse X JAM driver. Send 'help' for command list.");
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println(" num N              -> show number on big digits");
  Serial.println(" digit X Y          -> show digit Y on position X (1-6)");
  Serial.println(" digits a b c d e f -> show 6 digits (-1 = blank)");
  Serial.println(" set X Y / clr X Y  -> toggle byte X bit Y");
  Serial.println(" all 1 / all 0 / clear");
  Serial.println(" e N                -> energy = N (0-199)");
  Serial.println(" j N                -> juice  = N (0-199)");
  Serial.println(" 100                -> 100 in both areas");
  Serial.println(" energy on|off  juice on|off");
  Serial.println(" jam top|bot|center|full|off");
  Serial.println(" stars on|off|left|right");
  Serial.println(" anim off|sweep|trail|sparkle  /  animspeed N  /  traillen N");
}

void loop() {
  tickAnim();

  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh >= 20) {
    write_pls916h(frame);
    lastRefresh = millis();
  }

  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if      (cmd == "help")  printHelp();
  else if (cmd.startsWith("num ")) { displayNumber(cmd.substring(4).toInt());   Serial.println("OK"); }
  else if (cmd.startsWith("digit ")) {
    int pos, val; sscanf(cmd.c_str(), "digit %d %d", &pos, &val);
    clearDigits(); displayDigit(pos - 1, val); Serial.println("OK");
  }
  else if (cmd.startsWith("digits ")) {
    int a,b,c,d,e,f;
    sscanf(cmd.c_str(), "digits %d %d %d %d %d %d", &a,&b,&c,&d,&e,&f);
    displayDigits(a,b,c,d,e,f); Serial.println("OK");
  }
  else if (cmd.startsWith("set ")) {
    int x, y; sscanf(cmd.c_str(), "set %d %d", &x, &y); setBit(x, y, true);  Serial.println("OK");
  }
  else if (cmd.startsWith("clr ")) {
    int x, y; sscanf(cmd.c_str(), "clr %d %d", &x, &y); setBit(x, y, false); Serial.println("OK");
  }
  else if (cmd == "all 1") { memset(frame, 0xFF, sizeof(frame)); Serial.println("All ON"); }
  else if (cmd == "all 0") { memset(frame, 0x00, sizeof(frame)); Serial.println("All OFF"); }
  else if (cmd == "clear") { clearDigits(); Serial.println("Cleared"); }
  // Jam-specific
  else if (cmd.startsWith("e ")) { displayArea(0, cmd.substring(2).toInt()); Serial.println("OK"); }
  else if (cmd.startsWith("j ")) { displayArea(1, cmd.substring(2).toInt()); Serial.println("OK"); }
  else if (cmd == "100") { displayArea(0, 100); displayArea(1, 100); Serial.println("100 / 100"); }
  else if (cmd == "energy on")  { setBit(JAM_ENERGY_ICON, 7, true);  Serial.println("OK"); }
  else if (cmd == "energy off") { setBit(JAM_ENERGY_ICON, 7, false); Serial.println("OK"); }
  else if (cmd == "juice on")   { setBit(JAM_JUICE_ICON,  7, true);  Serial.println("OK"); }
  else if (cmd == "juice off")  { setBit(JAM_JUICE_ICON,  7, false); Serial.println("OK"); }
  else if (cmd == "jam top")    { fillRange(JAM_TOPBAR_FIRST, JAM_TOPBAR_LAST, true);  Serial.println("OK"); }
  else if (cmd == "jam bot")    { fillRange(JAM_BOTBAR_FIRST, JAM_BOTBAR_LAST, true);  Serial.println("OK"); }
  else if (cmd == "jam center") { fillRange(JAM_CENTER_FIRST, JAM_CENTER_LAST, true);  Serial.println("OK"); }
  else if (cmd == "jam full")   { fillRange(JAM_TOPBAR_FIRST, JAM_CENTER_LAST, true);  Serial.println("OK"); }
  else if (cmd == "jam off")    { fillRange(JAM_TOPBAR_FIRST, JAM_CENTER_LAST, false); Serial.println("OK"); }
  else if (cmd == "stars on")   { fillRange(JAM_STARS_FIRST, JAM_STARS_LAST, true);  Serial.println("OK"); }
  else if (cmd == "stars off")  { fillRange(JAM_STARS_FIRST, JAM_STARS_LAST, false); Serial.println("OK"); }
  else if (cmd == "stars left") {
    fillRange(JAM_STARS_FIRST, JAM_STARS_RIGHT - 1, true);
    fillRange(JAM_STARS_RIGHT, JAM_STARS_LAST,     false);
    Serial.println("OK");
  }
  else if (cmd == "stars right") {
    fillRange(JAM_STARS_FIRST, JAM_STARS_RIGHT - 1, false);
    fillRange(JAM_STARS_RIGHT, JAM_STARS_LAST,     true);
    Serial.println("OK");
  }
  else if (cmd == "anim off" || cmd == "anim none") { animMode = ANIM_NONE; clearStars(); Serial.println("OK"); }
  else if (cmd == "anim sweep")   { animMode = ANIM_SWEEP;   animPos = 0; animNext = 0; Serial.println("OK"); }
  else if (cmd == "anim trail")   { animMode = ANIM_TRAIL;   animPos = 0; animNext = 0; Serial.println("OK"); }
  else if (cmd == "anim sparkle") { animMode = ANIM_SPARKLE; animNext = 0; Serial.println("OK"); }
  else if (cmd.startsWith("animspeed ")) {
    int ms = cmd.substring(10).toInt();
    if (ms >= 20 && ms <= 5000) { animStep = ms; Serial.println("OK"); }
  }
  else if (cmd.startsWith("traillen ")) {
    int n = cmd.substring(9).toInt();
    if (n >= 1 && n <= 50) { animTrailLen = n; Serial.println("OK"); }
  }
  else Serial.println("Unknown command (try 'help')");
}
