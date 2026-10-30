#include "screen.h"

#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "generated/fonts_gen.h"

static const int X0 = 292, XR = 776;   // right-hand column
static const int LOGO_CX = 136;        // centre of the logo panel

// ---- small helpers -----------------------------------------------------------

// Draw text, cutting it short with ".." if it would run past maxw.
static void textFit(Canvas &c, int x, int y, const char *s, const Font &f, int maxw, int track = 0) {
  char buf[64];
  strncpy(buf, s, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  if (Canvas::textWidth(buf, f, track) > maxw) {
    size_t n = strlen(buf);
    while (n > 0) {
      buf[--n] = 0;
      char t[68];
      snprintf(t, sizeof(t), "%s..", buf);
      if (Canvas::textWidth(t, f, track) <= maxw) {
        strcpy(buf, t);
        break;
      }
    }
  }
  c.text(x, y, buf, f, track);
}

static void upper(char *dst, size_t n, const char *s) {
  size_t i = 0;
  for (; s[i] && i < n - 1; i++) dst[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];
  dst[i] = 0;
}

static void commas(char *out, size_t n, int32_t v) {
  char raw[16];
  snprintf(raw, sizeof(raw), "%ld", (long)(v < 0 ? -v : v));
  int len = strlen(raw), o = 0;
  if (v < 0 && o < (int)n - 1) out[o++] = '-';
  for (int i = 0; i < len && o < (int)n - 1; i++) {
    out[o++] = raw[i];
    if ((len - i - 1) % 3 == 0 && i < len - 1 && o < (int)n - 1) out[o++] = ',';
  }
  out[o] = 0;
}

// Small filled arrow pointing along `bearing` (0 = up), like mockup.arrow().
static void arrow(Canvas &c, float cx, float cy, float bearing, float r = 7) {
  float p[8] = {0, -r, r * 0.75f, r, 0, r * 0.45f, -r * 0.75f, r};
  float a = bearing * (float)M_PI / 180, ca = cosf(a), sa = sinf(a);
  for (int i = 0; i < 4; i++) {
    float x = p[2 * i], y = p[2 * i + 1];
    p[2 * i] = cx + x * ca - y * sa;
    p[2 * i + 1] = cy + x * sa + y * ca;
  }
  c.fillPolygon(p, 4);
}

static void routeArrow(Canvas &c, int x0, int x1, int y) {
  c.line(x0, y, x1 - 4, y, 5);
  float head[6] = {(float)x1, (float)y, (float)x1 - 18, (float)y - 12, (float)x1 - 18, (float)y + 12};
  c.fillPolygon(head, 3);
}

// ---- silhouettes (top-down, nose up, in a 200x200 box at ox,oy) -------------

static void poly(Canvas &c, int ox, int oy, std::initializer_list<float> pts, bool black = true) {
  float xy[32];
  int n = 0;
  for (float v : pts) xy[n] = v + ((n % 2) ? oy : ox), n++;
  c.fillPolygon(xy, n / 2, black);
}

static void mirrored(Canvas &c, int ox, int oy, std::initializer_list<float> pts) {
  poly(c, ox, oy, pts);
  float xy[32];
  int n = 0;
  for (float v : pts) xy[n] = (n % 2) ? v + oy : (200 - v) + ox, n++;
  c.fillPolygon(xy, n / 2);
}

static void silhouette(Canvas &c, const char *shape, int ox, int oy) {
  if (!strcmp(shape, "ga")) {                       // high-wing single prop (mockup.plane_icon)
    c.fillRect(ox + 92, oy + 26, 17, 148);
    c.fillCircle(ox + 100, oy + 26, 8);
    c.fillCircle(ox + 100, oy + 174, 8);
    poly(c, ox, oy, {12, 62, 188, 62, 188, 84, 12, 84});
    poly(c, ox, oy, {56, 160, 144, 160, 140, 176, 60, 176});
    c.fillCircle(ox + 100, oy + 20, 10);
  } else if (!strcmp(shape, "heli")) {
    c.fillCircle(ox + 100, oy + 82, 30);           // cabin
    c.fillRect(ox + 95, oy + 100, 10, 80);         // tail boom
    poly(c, ox, oy, {80, 172, 120, 172, 120, 182, 80, 182});
    c.line(ox + 22, oy + 18, ox + 178, oy + 146, 7);   // rotor
    c.line(ox + 178, oy + 18, ox + 22, oy + 146, 7);
  } else {
    bool wide = !strcmp(shape, "widebody");
    float fw = wide ? 13 : 9;                       // fuselage half-width
    c.fillRect(ox + 100 - fw, oy + 26, 2 * fw + 1, 150);
    c.fillCircle(ox + 100, oy + 26, fw);
    poly(c, ox, oy, {100 - fw, 176, 100 + fw, 176, 100, 194});
    float span = wide ? 98 : 90;
    mirrored(c, ox, oy, {100, 72, 100 - span, 122, 100 - span, 134, 100, 106});   // wings
    mirrored(c, ox, oy, {100, 156, 58, 180, 58, 188, 100, 178});                  // tailplane
    float e = wide ? 60 : 58, ew = wide ? 14 : 10, eh = wide ? 22 : 18;          // engines
    float lead = 72 + (100 - e) * 50 / span;       // wing leading edge at this x
    c.fillRect(ox + e - ew / 2, oy + lead - eh + 6, ew, eh);
    c.fillRect(ox + 200 - e - ew / 2, oy + lead - eh + 6, ew, eh);
  }
}

// ---- stats row ---------------------------------------------------------------

enum Indicator { NONE, UP, DOWN, LEVEL, BEARING };

static void statCell(Canvas &c, int x, int y, bool divider, const char *label, const char *value,
                     const char *unit, Indicator ind, float bearing = 0) {
  if (divider) c.vline(x - 10, y + 2, y + 50);
  int lw = c.text(x, y, label, FONT_LABEL, 2);
  float ix = x + lw + 14, iy = y + 7;
  switch (ind) {
    case UP: arrow(c, ix, iy, 0); break;
    case DOWN: arrow(c, ix, iy, 180); break;
    case LEVEL: c.line(ix - 7, iy, ix + 7, iy, 3); break;
    case BEARING: arrow(c, ix, iy, bearing); break;
    default: break;
  }
  int vw = c.text(x, y + 18, value, FONT_VALUE);
  if (unit && *unit) c.text(x + vw + 4, y + 30, unit, FONT_UNIT);
}

// ---- screens -----------------------------------------------------------------

void drawFlight(Canvas &c, const Flight &f, const char *caption) {
  c.clear();

  // Left: logo panel. No logos yet (phase 3): a silhouette by aircraft class.
  silhouette(c, f.shape[0] ? f.shape : "narrowbody", 36, 14);
  int cw = Canvas::textWidth(caption, FONT_CAPTION, 2);
  c.text(LOGO_CX - cw / 2, 248, caption, FONT_CAPTION, 2);
  c.fillRect(272, 14, 2, 245);

  // Header: airline (or what we know instead) + flight number.
  char head[40];
  if (f.hasAirline) upper(head, sizeof(head), f.airlineName);
  else if (strlen(f.callsign) > 3 && f.callsign[0] >= 'A' && f.callsign[3] >= '0' && f.callsign[3] <= '9' &&
           f.callsign[1] >= 'A' && f.callsign[2] >= 'A')
    snprintf(head, sizeof(head), "%.3s", f.callsign);          // airline we don't know the name of
  else strcpy(head, !strcmp(f.shape, "heli") ? "HELICOPTER" : "PRIVATE");
  const char *num = f.flight[0] ? f.flight : f.reg;
  if (f.emergency) {
    c.fillRect(X0 - 8, 8, XR - X0 + 16, 40);
    char em[32];
    snprintf(em, sizeof(em), "EMERGENCY %s", f.squawk);
    c.text(X0, 14, em, FONT_AIRLINE, 4, false);
    c.textRight(XR, 12, num, FONT_FLIGHT, 0, false);
  } else {
    int nw = Canvas::textWidth(num, FONT_FLIGHT);
    textFit(c, X0, 14, head, FONT_AIRLINE, XR - X0 - nw - 20, 4);
    c.textRight(XR, 12, num, FONT_FLIGHT);
  }

  // Route
  if (f.hasRoute) {
    const char *to = f.hasTo ? f.to.code : "";
    // Shrink to the smaller display font if 4-letter codes won't fit.
    const Font *big = &FONT_ROUTE;
    int y = 44, ay = 88;
    int need = Canvas::textWidth(f.from.code, *big) + 114 + (f.hasTo ? Canvas::textWidth(to, *big) : 170);
    if (need > XR - X0) big = &FONT_BIG, y = 58, ay = 88;
    c.text(X0 - 4, y, f.from.code, *big);
    int fw = Canvas::textWidth(f.from.code, *big);
    int ax0 = X0 + fw + 18, ax1 = X0 + fw + 96;
    routeArrow(c, ax0, ax1, ay);
    textFit(c, X0, 138, f.from.city, FONT_CITY, ax1 + 22 - X0 - 14);
    if (f.hasTo) {
      c.text(ax1 + 18, y, to, *big);
      bool doubt = !strcmp(f.basis, "mismatch");
      int nw = doubt ? Canvas::textWidth("UNCONFIRMED", FONT_LABEL, 2) + 12 : 0;
      textFit(c, ax1 + 22, 138, f.to.city, FONT_CITY, XR - ax1 - 22 - nw);
      if (doubt) c.textRight(XR, 142, "UNCONFIRMED", FONT_LABEL, 2);
    } else {
      c.text(ax1 + 22, 72, "DESTINATION", FONT_NOTE, 3);
      c.text(ax1 + 22, 92, "NOT YET KNOWN", FONT_NOTE, 3);
    }
  } else {
    c.text(X0, 86, "ROUTE NOT KNOWN", FONT_AIRLINE, 4);
  }

  // Aircraft line
  char ac[80];
  if (f.reg[0] && strcmp(f.reg, num)) snprintf(ac, sizeof(ac), "%s  \xC2\xB7  %s", f.typeName, f.reg);
  else snprintf(ac, sizeof(ac), "%s", f.typeName[0] ? f.typeName : "Unknown type");
  textFit(c, X0, 168, ac, FONT_AIRCRAFT, XR - X0);

  c.hline(X0, XR, 202);

  // Stats
  char v[24];
  // Uneven columns: altitude needs room for "36,000 ft".
  const int y = 210, cx[4] = {X0, X0 + 138, X0 + 252, X0 + 372};
  if (f.altFt != NO_VALUE) commas(v, sizeof(v), f.altFt); else strcpy(v, "--");
  Indicator vi = !strcmp(f.vtrend, "up") ? UP : !strcmp(f.vtrend, "down") ? DOWN : LEVEL;
  statCell(c, cx[0], y, false, "ALTITUDE", v, "ft", vi);
  if (f.gsKt != NO_VALUE) snprintf(v, sizeof(v), "%ld", (long)f.gsKt); else strcpy(v, "--");
  statCell(c, cx[1], y, true, "SPEED", v, "kt", NONE);
  snprintf(v, sizeof(v), f.distNm < 10 ? "%.1f" : "%.0f", f.distNm);
  statCell(c, cx[2], y, true, "DISTANCE", v, "nm", BEARING, f.bearingDeg);
  if (f.trackDeg != NO_VALUE) snprintf(v, sizeof(v), "%03ld\xC2\xB0", (long)f.trackDeg); else strcpy(v, "--");
  statCell(c, cx[3], y, true, "TRACK", v, "", NONE);
}

void drawMessage(Canvas &c, const char *title, const char *line1, const char *line2,
                 const char *footer) {
  c.clear();
  // Left: a small radar scope.
  for (int r = 96; r > 0; r -= 32) {
    c.fillCircle(LOGO_CX, 126, r);
    c.fillCircle(LOGO_CX, 126, r - 2, false);
  }
  c.fillCircle(LOGO_CX, 126, 5);
  c.line(LOGO_CX, 126, LOGO_CX + 62, 126 - 62, 2);
  c.fillRect(272, 14, 2, 245);

  textFit(c, X0, 34, title, FONT_BIG, XR - X0, 2);
  if (line1) textFit(c, X0, 118, line1, FONT_AIRCRAFT, XR - X0);
  if (line2) textFit(c, X0, 150, line2, FONT_CITY, XR - X0);
  c.hline(X0, XR, 202);
  if (footer) c.text(X0, 214, footer, FONT_LABEL, 2);
}
