#include "text.h"

// Latin-1 Supplement and Latin Extended-A letters -> their base letter.
// Index = code point - 0xC0. '\0' = drop.
static const char LATIN1[] =
    "AAAAAAACEEEEIIII" "DNOOOOO\0OUUUUYTs" "aaaaaaaceeeeiiii" "dnooooo\0ouuuuyty";
static const char LATINA[] =  // U+0100..U+017F
    "AaAaAaCcCcCcCcDd" "DdEeEeEeEeEeGgGg" "GgGgHhHhIiIiIiIi" "IiJjJjKkkLlLlLlL"
    "lLlNnNnNnnNnOoOo" "OoOoRrRrRrSsSsSs" "SsTtTtTtUuUuUuUu" "UuUuWwYyYZzZzZzs";

void asciiFold(char *s) {
  const unsigned char *r = (const unsigned char *)s;
  char *w = s;
  while (*r) {
    if (*r < 0x80) { *w++ = *r++; continue; }
    int len = (*r >= 0xF0) ? 4 : (*r >= 0xE0) ? 3 : (*r >= 0xC0) ? 2 : 1;
    uint32_t cp = *r & (0x7F >> len);
    int i = 1;
    for (; i < len && (r[i] & 0xC0) == 0x80; i++) cp = cp << 6 | (r[i] & 0x3F);
    r += i;
    if (cp == 0xC6 || cp == 0xE6) {   // Æ æ
      *w++ = cp == 0xC6 ? 'A' : 'a';
      *w++ = cp == 0xC6 ? 'E' : 'e';
      continue;
    }
    char c = 0;
    if (cp >= 0xC0 && cp < 0x100) c = LATIN1[cp - 0xC0];
    else if (cp >= 0x100 && cp < 0x180) c = LATINA[cp - 0x100];
    else if (cp == 0x2018 || cp == 0x2019) c = '\'';
    else if (cp == 0x2013 || cp == 0x2014) c = '-';
    if (c) *w++ = c;
  }
  *w = 0;
}

uint32_t crc32(const uint8_t *data, size_t len, uint32_t crc) {
  static uint32_t table[256];
  if (!table[1]) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1));
      table[i] = c;
    }
  }
  crc = ~crc;
  for (size_t i = 0; i < len; i++) crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFF];
  return ~crc;
}
