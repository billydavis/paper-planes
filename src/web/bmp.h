#pragma once
#include <WebServer.h>
#include <stdint.h>

// Send a 1-bit canvas frame (row-major, MSB first, 1 = black) as a BMP in
// e-paper colours, row by row (no full-size copy).
void sendBmp(WebServer &server, const uint8_t *canvas, int width, int height);
