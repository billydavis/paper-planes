#pragma once
#include <stdint.h>

// A 1-bit airline logo: row-major, MSB first, rows padded to whole bytes,
// 1 = black. Generated into generated/logos_gen.h by tools/logogen.py.
struct Logo {
  const char *icao;   // 3-letter operator code, e.g. "AAL"
  uint16_t w, h;
  const uint8_t *bits;
};

// The logo for an ICAO operator code, or nullptr if we have none.
const Logo *findLogo(const char *icao);
