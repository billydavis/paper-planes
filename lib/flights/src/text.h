#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Bounded copy into a fixed char array; always terminated, null src = "".
template <size_t N>
inline void scopy(char (&dst)[N], const char *src) {
  strncpy(dst, src ? src : "", N - 1);
  dst[N - 1] = 0;
}

// Fold UTF-8 accents to plain ASCII in place ("Aeroméxico" -> "Aeromexico"),
// dropping anything with no ASCII form. The panel's bitmap fonts are ASCII.
void asciiFold(char *s);

// CRC-32 (IEEE), for checking the airport data image.
uint32_t crc32(const uint8_t *data, size_t len, uint32_t crc = 0);
