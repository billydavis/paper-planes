#include "airports.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "geo.h"
#include "text.h"

AirportDb &airportDb() {
  static AirportDb db;
  return db;
}

bool AirportDb::attach(const uint8_t *image, size_t size) {
  detach();
  if (!image || size < sizeof(AirportImageHeader)) return false;
  AirportImageHeader h;
  memcpy(&h, image, sizeof(h));
  if (memcmp(h.magic, "APT1", 4) || h.version != AIRPORT_IMAGE_VERSION) return false;
  if (h.totalSize > size || h.count == 0 || h.count > 65535) return false;
  auto inside = [&](uint32_t off, uint64_t len) { return off >= sizeof(h) && off + len <= h.totalSize; };
  if (!inside(h.recordsOff, (uint64_t)h.count * sizeof(AirportRec)) ||
      !inside(h.indexOff, (uint64_t)h.count * 2) || !inside(h.poolOff, h.poolSize) || h.poolSize == 0)
    return false;
  if (h.recordsOff % 4 || h.indexOff % 2) return false;
  if (crc32(image + sizeof(h), h.totalSize - sizeof(h)) != h.crc) return false;
  if (image[h.poolOff + h.poolSize - 1] != 0) return false;   // pool ends terminated
  recs = (const AirportRec *)(image + h.recordsOff);
  index = (const uint16_t *)(image + h.indexOff);
  pool = (const char *)(image + h.poolOff);
  poolSize = h.poolSize;
  n = h.count;
  return true;
}

void AirportDb::detach() {
  n = 0;
  recs = nullptr;
  index = nullptr;
  pool = nullptr;
}

// Offsets are checked here too, so even a corrupt-but-CRC-valid image (or one
// being rewritten under us) can't send a read outside the pool.
const char *AirportDb::keyOf(int rec) const {
  uint32_t off = recs[rec].meta >> 2;
  return off < poolSize ? pool + off : "";
}

void AirportDb::fill(int rec, Stub &out) const {
  const char *key = keyOf(rec);
  const char *end = pool + poolSize;
  const char *iata = key + strnlen(key, end - key) + 1;
  const char *city = iata < end ? iata + strnlen(iata, end - iata) + 1 : end - 1;
  if (iata >= end) iata = end - 1;
  if (city >= end) city = end - 1;
  out = Stub();
  scopy(out.code, iata[0] ? iata : key);
  scopy(out.city, city);
  out.hasPos = true;
  out.lat = recs[rec].lat / 1e6;
  out.lon = recs[rec].lon / 1e6;
}

bool AirportDb::nearest(double lat, double lon, double maxNm, uint8_t kinds, Stub &out, double toward,
                        double coneDeg) const {
  int count = n;
  if (!count) return false;
  double box = maxNm / 60.0 + 0.05;
  double lonBox = box / fmax(cos(lat * M_PI / 180), 0.01);
  int32_t lo = (int32_t)((lat - box) * 1e6), hi = (int32_t)((lat + box) * 1e6);
  // Records are sorted by latitude: jump to the start of the band.
  int a = 0, b = count;
  while (a < b) {
    int m = (a + b) / 2;
    if (recs[m].lat < lo) a = m + 1; else b = m;
  }
  int best = -1;
  double bestNm = maxNm;
  for (int i = a; i < count && recs[i].lat <= hi; i++) {
    const AirportRec &r = recs[i];
    if (!(kinds & (1 << (r.meta & 3)))) continue;
    double alon = r.lon / 1e6;
    if (fabs(alon - lon) > lonBox) continue;
    double alat = r.lat / 1e6;
    double d = gcNm(lat, lon, alat, alon);
    if (d >= bestNm) continue;
    if (toward >= 0 && angleOff(bearingDeg(lat, lon, alat, alon), toward) > coneDeg) continue;
    best = i;
    bestNm = d;
  }
  if (best < 0) return false;
  fill(best, out);
  return true;
}

Stub AirportDb::byCode(const char *code) const {
  Stub s;
  char key[8];
  size_t len = 0;
  for (; code && code[len] && len < sizeof(key) - 1; len++) key[len] = toupper((unsigned char)code[len]);
  key[len] = 0;
  int count = n;
  int a = 0, b = count - 1;
  while (a <= b) {
    int m = (a + b) / 2;
    int rec = index[m];
    if (rec >= count) break;
    int c = strcmp(keyOf(rec), key);
    if (c == 0) { fill(rec, s); return s; }
    if (c < 0) a = m + 1; else b = m - 1;
  }
  scopy(s.code, key);
  return s;
}

bool sameField(const Stub &a, const Stub &b) {
  return a.hasPos && b.hasPos && gcNm(a.lat, a.lon, b.lat, b.lon) < 30;
}
