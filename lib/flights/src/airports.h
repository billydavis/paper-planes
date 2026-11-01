#pragma once
#include <stddef.h>
#include <stdint.h>

// Airport lookups, same rules as flight-tracker's airports.py, over a binary
// image tools/datagen.py builds from flight-tracker's data/airports.csv.
// On the board the image lives in its own flash partition ("airports") and
// is memory-mapped; tests load it from src/generated/airports.bin.

// Image layout (little-endian). Every offset is from the start of the image.
struct AirportImageHeader {
  char magic[4];        // "APT1"
  uint32_t version;     // AIRPORT_IMAGE_VERSION
  uint32_t count;       // airports
  uint32_t recordsOff;  // AirportRec[count], sorted by latitude
  uint32_t indexOff;    // uint16_t[count]: record numbers sorted by key
  uint32_t poolOff;     // "KEY\0IATA\0CITY\0" per airport
  uint32_t poolSize;
  uint32_t totalSize;   // header + everything after it
  uint32_t crc;         // CRC-32 of bytes [sizeof(header), totalSize)
};
static const uint32_t AIRPORT_IMAGE_VERSION = 1;

struct AirportRec {
  int32_t lat, lon;     // millionths of a degree
  uint32_t meta;        // type (bits 0-1: 0 large, 1 medium, 2 small) | pool offset << 2
};

// An airport as the route logic passes it around (airport_stub in Python).
struct Stub {
  char code[8] = "";    // IATA when it has one, else ICAO/ident
  char city[40] = "";
  bool hasPos = false;
  double lat = 0, lon = 0;
};

enum AirportKinds : uint8_t { AP_LARGE = 1, AP_MEDIUM = 2, AP_SMALL = 4,
                              AP_BIG = AP_LARGE | AP_MEDIUM, AP_ANY = 7 };

class AirportDb {
 public:
  // Use `image` (not copied; must stay valid). Checks the header, bounds and
  // CRC; returns false (and stays empty) if anything is off.
  bool attach(const uint8_t *image, size_t size);
  void detach();
  bool ready() const { return n > 0; }
  int count() const { return n; }

  // Closest airport of the given kinds within maxNm. With toward >= 0, only
  // airports within coneDeg of that heading from the point count.
  bool nearest(double lat, double lon, double maxNm, uint8_t kinds, Stub &out,
               double toward = -1, double coneDeg = 55) const;
  // ICAO code / ident -> stub. Unknown: just the code, no position.
  Stub byCode(const char *code) const;

 private:
  const char *keyOf(int rec) const;
  void fill(int rec, Stub &out) const;
  const AirportRec *recs = nullptr;
  const uint16_t *index = nullptr;
  const char *pool = nullptr;
  uint32_t poolSize = 0;
  volatile int n = 0;
};

// The one database the firmware uses.
AirportDb &airportDb();

// Two stubs within 30 nm of each other: the same field, near enough.
bool sameField(const Stub &a, const Stub &b);
