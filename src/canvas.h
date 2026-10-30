#pragma once
#include <stdint.h>
#include "font.h"

// A 792x272 1-bit drawing surface in screen coordinates (0,0 = top left,
// x across the long side). Bit set = black. Kept separate from the driver's
// buffer so frames can be compared, dumped over serial, and copied across
// the gap between the panel's two controller chips in one place.
class Canvas {
 public:
  static const int W = 792, H = 272, STRIDE = W / 8;

  void clear();
  void set(int x, int y, bool black = true);
  bool get(int x, int y) const;

  void fillRect(int x, int y, int w, int h, bool black = true);
  void hline(int x0, int x1, int y, int width = 1);
  void vline(int x, int y0, int y1, int width = 1);
  void line(float x0, float y0, float x1, float y1, float width);
  void fillPolygon(const float *xy, int n, bool black = true);
  void fillCircle(float cx, float cy, float r, bool black = true);

  // Text with the top of the line at y (like Pillow's ImageDraw.text).
  // `track` adds that many pixels between characters. Returns the width.
  int text(int x, int y, const char *s, const Font &f, int track = 0, bool black = true);
  int textRight(int xr, int y, const char *s, const Font &f, int track = 0, bool black = true);
  static int textWidth(const char *s, const Font &f, int track = 0);

  // Copy into the Elecrow driver's buffer (via Paint_SetPixel, which knows
  // about rotation and the chip gap).
  void toDriver() const;

  bool operator==(const Canvas &o) const;
  bool operator!=(const Canvas &o) const { return !(*this == o); }

  uint8_t buf[STRIDE * H];
};
