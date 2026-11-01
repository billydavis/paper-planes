#include "panel_layout.h"

// Canvas column x lands at x' = x (+8 past the chip gap); the driver stores
// x' mirrored (X = 799 - x') on row 271 - y. Column 396 is bit 3 of canvas
// byte 49, so the gap splits that byte: its high nibble stays in byte 49,
// its low nibble moves to byte 50, and later bytes shift by exactly one.

static uint8_t reverseBits(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  return (b & 0xAA) >> 1 | (b & 0x55) << 1;
}

static const uint8_t *revTable() {
  static uint8_t t[256];
  static bool ready = false;
  if (!ready) {
    for (int i = 0; i < 256; i++) t[i] = reverseBits(i);
    ready = true;
  }
  return t;
}

void canvasToDriver(const uint8_t *canvas, uint8_t *driver) {
  const uint8_t *rev = revTable();
  uint8_t t[DRIVER_STRIDE];   // one row in x' order, 1 = black
  for (int y = 0; y < PANEL_H; y++) {
    const uint8_t *c = canvas + y * CANVAS_STRIDE;
    for (int i = 0; i < 49; i++) t[i] = c[i];
    t[49] = c[49] & 0xF0;
    t[50] = c[49] & 0x0F;
    for (int i = 50; i < CANVAS_STRIDE; i++) t[i + 1] = c[i];
    uint8_t *d = driver + (PANEL_H - 1 - y) * DRIVER_STRIDE;
    for (int j = 0; j < DRIVER_STRIDE; j++) d[j] = ~rev[t[DRIVER_STRIDE - 1 - j]];
  }
}

void driverToCanvas(const uint8_t *driver, uint8_t *canvas) {
  const uint8_t *rev = revTable();
  uint8_t t[DRIVER_STRIDE];
  for (int y = 0; y < PANEL_H; y++) {
    const uint8_t *d = driver + (PANEL_H - 1 - y) * DRIVER_STRIDE;
    for (int k = 0; k < DRIVER_STRIDE; k++) t[k] = ~rev[d[DRIVER_STRIDE - 1 - k]];
    uint8_t *c = canvas + y * CANVAS_STRIDE;
    for (int i = 0; i < 49; i++) c[i] = t[i];
    c[49] = (t[49] & 0xF0) | (t[50] & 0x0F);
    for (int i = 50; i < CANVAS_STRIDE; i++) c[i] = t[i + 1];
  }
}
