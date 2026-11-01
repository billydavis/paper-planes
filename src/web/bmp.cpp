#include "web/bmp.h"

#include <string.h>

void sendBmp(WebServer &server, const uint8_t *canvas, int width, int height) {
  const int stride = (width + 7) / 8;
  const int rowBytes = (width + 31) / 32 * 4;   // BMP rows are padded to 4 bytes
  const uint32_t dataSize = rowBytes * height, fileSize = 62 + dataSize;
  uint8_t hdr[62] = {'B', 'M'};
  auto le32 = [&](int at, uint32_t v) { for (int i = 0; i < 4; i++) hdr[at + i] = v >> (8 * i); };
  le32(2, fileSize);
  le32(10, 62);         // pixel data offset
  le32(14, 40);         // BITMAPINFOHEADER
  le32(18, width);
  le32(22, height);     // positive = rows stored bottom-up
  hdr[26] = 1;          // planes
  hdr[28] = 1;          // bits per pixel
  le32(34, dataSize);
  le32(46, 2);          // palette entries
  // Palette (B, G, R, 0): 0 = paper, 1 = ink.
  const uint8_t pal[8] = {0xE4, 0xEA, 0xEC, 0, 0x26, 0x24, 0x22, 0};
  memcpy(hdr + 54, pal, 8);

  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(fileSize);
  server.send(200, "image/bmp", "");
  server.sendContent((const char *)hdr, sizeof(hdr));
  uint8_t row[256];
  memset(row, 0, sizeof(row));
  for (int y = height - 1; y >= 0; y--) {
    memcpy(row, canvas + y * stride, stride);
    server.sendContent((const char *)row, rowBytes);
  }
}
