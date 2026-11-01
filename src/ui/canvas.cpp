#include "ui/canvas.h"

#include <math.h>
#include <string.h>



void Canvas::clear() { memset(buf, 0, sizeof(buf)); }

void Canvas::set(int x, int y, bool black) {
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  uint8_t m = 0x80 >> (x & 7);
  if (black) buf[y * STRIDE + (x >> 3)] |= m;
  else buf[y * STRIDE + (x >> 3)] &= ~m;
}

bool Canvas::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= W || y >= H) return false;
  return buf[y * STRIDE + (x >> 3)] & (0x80 >> (x & 7));
}

void Canvas::fillRect(int x, int y, int w, int h, bool black) {
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++) set(i, j, black);
}

void Canvas::hline(int x0, int x1, int y, int width) {
  fillRect(x0, y - (width - 1) / 2, x1 - x0 + 1, width);
}

void Canvas::vline(int x, int y0, int y1, int width) {
  fillRect(x - (width - 1) / 2, y0, width, y1 - y0 + 1);
}

void Canvas::line(float x0, float y0, float x1, float y1, float width) {
  float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
  if (len == 0) return;
  float nx = -dy / len * width / 2, ny = dx / len * width / 2;
  float q[8] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
  fillPolygon(q, 4);
}

// Scanline fill, sampling at pixel centres (even-odd rule).
void Canvas::fillPolygon(const float *xy, int n, bool black) {
  float ymin = 1e9, ymax = -1e9;
  for (int i = 0; i < n; i++) {
    ymin = fminf(ymin, xy[2 * i + 1]);
    ymax = fmaxf(ymax, xy[2 * i + 1]);
  }
  float xs[16];
  for (int y = (int)floorf(ymin); y <= (int)ceilf(ymax); y++) {
    float sy = y + 0.5f;
    int k = 0;
    for (int i = 0; i < n && k < 16; i++) {
      float ax = xy[2 * i], ay = xy[2 * i + 1];
      float bx = xy[2 * ((i + 1) % n)], by = xy[2 * ((i + 1) % n) + 1];
      if ((ay <= sy && by > sy) || (by <= sy && ay > sy))
        xs[k++] = ax + (sy - ay) / (by - ay) * (bx - ax);
    }
    for (int a = 1; a < k; a++)  // insertion sort, k is tiny
      for (int b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
        float t = xs[b]; xs[b] = xs[b - 1]; xs[b - 1] = t;
      }
    for (int a = 0; a + 1 < k; a += 2)
      for (int x = (int)ceilf(xs[a] - 0.5f); x <= (int)floorf(xs[a + 1] - 0.5f); x++)
        set(x, y, black);
  }
}

void Canvas::fillCircle(float cx, float cy, float r, bool black) {
  for (int y = (int)(cy - r); y <= (int)(cy + r) + 1; y++)
    for (int x = (int)(cx - r); x <= (int)(cx + r) + 1; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      if (dx * dx + dy * dy <= r * r) set(x, y, black);
    }
}

// Decode one UTF-8 codepoint and advance *s.
static uint32_t nextCp(const char **s) {
  const uint8_t *p = (const uint8_t *)*s;
  uint32_t c = *p++;
  if (c >= 0xE0 && p[0] && p[1]) {
    c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
    p += 2;
  } else if (c >= 0xC0 && p[0]) {
    c = ((c & 0x1F) << 6) | (p[0] & 0x3F);
    p += 1;
  }
  *s = (const char *)p;
  return c;
}

int Canvas::textWidth(const char *s, const Font &f, int track) {
  int32_t pen = 0;
  int n = 0;
  while (*s) {
    int gi = glyphIndex(nextCp(&s));
    if (gi < 0) gi = glyphIndex('?');
    pen += f.glyphs[gi].adv64 + track * 64;
    n++;
  }
  if (n) pen -= track * 64;
  return (pen + 32) >> 6;
}

int Canvas::text(int x, int y, const char *s, const Font &f, int track, bool black) {
  int32_t pen = x * 64;
  int n = 0;
  while (*s) {
    int gi = glyphIndex(nextCp(&s));
    if (gi < 0) gi = glyphIndex('?');
    const Glyph &g = f.glyphs[gi];
    int gx = ((pen + 32) >> 6) + g.x0, gy = y + g.y0;
    uint32_t bit = 0;
    for (int j = 0; j < g.h; j++)
      for (int i = 0; i < g.w; i++, bit++)
        if (f.bits[g.offset + (bit >> 3)] & (0x80 >> (bit & 7))) set(gx + i, gy + j, black);
    pen += g.adv64 + track * 64;
    n++;
  }
  if (n) pen -= track * 64;
  return ((pen + 32) >> 6) - x;
}

int Canvas::textRight(int xr, int y, const char *s, const Font &f, int track, bool black) {
  int w = textWidth(s, f, track);
  text(xr - w, y, s, f, track, black);
  return w;
}

bool Canvas::operator==(const Canvas &o) const { return memcmp(buf, o.buf, sizeof(buf)) == 0; }
