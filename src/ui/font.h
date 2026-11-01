#pragma once
#include <stdint.h>

// One glyph of a 1-bit bitmap font made by tools/fontgen.py. Bits are packed
// row-major, MSB first, with no padding between rows.
struct Glyph {
  uint32_t offset;   // into Font::bits
  uint8_t w, h;      // bitmap size
  int8_t x0, y0;     // bitmap position relative to pen x and the top of the line
  uint16_t adv64;    // pen advance, in 1/64 px
};

struct Font {
  const uint8_t *bits;
  const Glyph *glyphs;
  uint8_t ascent, descent;
};

// Index of a Unicode codepoint in a font's glyph table, or -1.
// Table order: ASCII 32..126, then degree sign, middle dot, em dash.
inline int glyphIndex(uint32_t cp) {
  if (cp >= 32 && cp <= 126) return cp - 32;
  if (cp == 0xB0) return 95;
  if (cp == 0xB7) return 96;
  if (cp == 0x2014) return 97;
  return -1;
}
