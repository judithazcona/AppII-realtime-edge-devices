// led_test.ino — find which GPIO drives the board's single on-board RGB LED.
//
// WHY THIS EXISTS: the board has one addressable RGB LED that must carry all
// four states (green / red / blue / magenta), and its data GPIO is not printed
// anywhere we can read. We refuse to guess it, so we drive the two plausible
// candidates one after the other and watch which one answers.
//
// NO SERIAL IN THIS SKETCH AT ALL — not even a Serial.begin(). It has to prove
// the LED works on its own, exactly as the graded firmware must (AGENTS.md
// rule 7 and rule 11: USB is power only in the demo, the firmware never waits
// for a host).
//
// HOW TO READ IT (timing is the only channel, so it is deliberately slow):
//
//   block 1, 12 s  -> GPIO 48 :  red, green, blue, magenta, 3 s each
//   gap,     4 s   -> both pins idle (LED dark)  = "switching pins now"
//   block 2, 12 s  -> GPIO 38 :  red, green, blue, magenta, 3 s each
//   gap,     4 s   -> dark
//   ... repeats forever
//
// Whichever block lights the LED is the answer. No light at all in either block
// means the LED is on another GPIO, or it is not an addressable part: report
// that, do not pick a pin anyway.

#include <Adafruit_NeoPixel.h>

#include "../config.h"

static const uint8_t CANDIDATE_PINS[] = {48, 38};
static const uint8_t NUM_CANDIDATES =
    sizeof(CANDIDATE_PINS) / sizeof(CANDIDATE_PINS[0]);

static const unsigned long COLOUR_MS = 3000;  // each colour
static const unsigned long GAP_MS = 4000;     // dark between pins

// Colour order we want the observer to see, and the values sent for each.
static const uint32_t COLOURS[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFF00FF};
static const char *COLOUR_NAMES[] = {"red", "green", "blue", "magenta"};
static const uint8_t NUM_COLOURS = sizeof(COLOURS) / sizeof(COLOURS[0]);

static Adafruit_NeoPixel strip[2];

void setup() {
  for (uint8_t i = 0; i < NUM_CANDIDATES; ++i) {
    strip[i] = Adafruit_NeoPixel(RGB_LED_COUNT, CANDIDATE_PINS[i], NEO_GRB + NEO_KHZ800);
    strip[i].begin();
    strip[i].setBrightness(RGB_LED_BRIGHTNESS);
    strip[i].clear();
    strip[i].show();
  }
  delay(500);
}

// Drive one candidate pin through all four colours, then go dark.
static void runBlock(Adafruit_NeoPixel &led) {
  const unsigned long start = millis();
  while (millis() - start < NUM_COLOURS * COLOUR_MS) {
    const uint8_t idx = (uint8_t)((millis() - start) / COLOUR_MS);
    if (idx < NUM_COLOURS) {
      for (uint8_t p = 0; p < RGB_LED_COUNT; ++p) led.setPixelColor(p, COLOURS[idx]);
      led.show();
    }
    delay(20);
  }
  led.clear();
  led.show();
}

static void goDark(unsigned long ms) {
  for (uint8_t i = 0; i < NUM_CANDIDATES; ++i) {
    strip[i].clear();
    strip[i].show();
  }
  delay(ms);
}

void loop() {
  for (uint8_t i = 0; i < NUM_CANDIDATES; ++i) {
    runBlock(strip[i]);
    goDark(GAP_MS);
  }
}
