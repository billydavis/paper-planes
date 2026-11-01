// The fast row-wise canvas <-> driver conversion (lib/panel) against the
// vendor driver's own per-pixel formula (Paint_SetPixel, Rotation 180, with
// the 8-pixel gap between the two controller chips).
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <initializer_list>

#include "panel_layout.h"

// lib/EPD_Driver/EPD.cpp Paint_SetPixel, case 180, Color BLACK.
static void driverSetBlack(uint8_t *img, int x, int y) {
  int xp = x >= 396 ? x + 8 : x;
  int X = 800 - xp - 1, Y = 272 - y - 1;
  img[X / 8 + Y * DRIVER_STRIDE] &= ~(0x80 >> (X % 8));
}

static void randomCanvas(uint8_t *c, unsigned seed) {
  srand(seed);
  for (int i = 0; i < CANVAS_BYTES; i++) c[i] = rand() & 0xFF;
}

static uint8_t canvas[CANVAS_BYTES], back[CANVAS_BYTES], fast[DRIVER_BYTES], ref[DRIVER_BYTES];

void setUp() {}
void tearDown() {}

static void matchesDriverFormula(unsigned seed) {
  randomCanvas(canvas, seed);
  memset(ref, 0xFF, sizeof(ref));
  for (int y = 0; y < PANEL_H; y++)
    for (int x = 0; x < PANEL_W; x++)
      if (canvas[y * CANVAS_STRIDE + x / 8] & (0x80 >> (x % 8))) driverSetBlack(ref, x, y);
  canvasToDriver(canvas, fast);
  TEST_ASSERT_EQUAL_MEMORY(ref, fast, DRIVER_BYTES);
}

void test_random_frames_match_driver() {
  for (unsigned seed = 1; seed <= 5; seed++) matchesDriverFormula(seed);
}

void test_single_pixels_at_the_chip_gap() {
  // Columns either side of the gap, where the canvas byte is split.
  for (int x : {0, 391, 392, 395, 396, 397, 399, 400, 791}) {
    memset(canvas, 0, sizeof(canvas));
    canvas[10 * CANVAS_STRIDE + x / 8] = 0x80 >> (x % 8);
    memset(ref, 0xFF, sizeof(ref));
    driverSetBlack(ref, x, 10);
    canvasToDriver(canvas, fast);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(ref, fast, DRIVER_BYTES, "column next to the gap");
  }
}

void test_gap_stays_white() {
  memset(canvas, 0xFF, sizeof(canvas));   // all black
  canvasToDriver(canvas, fast);
  // Driver columns 396..403 are the gap: byte 49 low nibble, byte 50 high nibble.
  for (int y = 0; y < PANEL_H; y++) {
    TEST_ASSERT_EQUAL_HEX8(0x0F, fast[y * DRIVER_STRIDE + 49] & 0x0F);
    TEST_ASSERT_EQUAL_HEX8(0xF0, fast[y * DRIVER_STRIDE + 50] & 0xF0);
  }
}

void test_round_trip() {
  randomCanvas(canvas, 42);
  canvasToDriver(canvas, fast);
  driverToCanvas(fast, back);
  TEST_ASSERT_EQUAL_MEMORY(canvas, back, CANVAS_BYTES);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_random_frames_match_driver);
  RUN_TEST(test_single_pixels_at_the_chip_gap);
  RUN_TEST(test_gap_stays_white);
  RUN_TEST(test_round_trip);
  return UNITY_END();
}
