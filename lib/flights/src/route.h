#pragma once
#include <ArduinoJson.h>
#include <stdint.h>

#include "airports.h"
#include "flight.h"

// The rules of flight-tracker's /api/epaper/nearest, without any I/O:
// epaper.py (pick, resolve_route, the response fields) and sources.py
// (route_fits, note_departure, _guess_dest). The board's sources.cpp feeds
// these with live HTTP results; the parity tests feed them recorded ones and
// compare with what the Python server answered.
//
// Function names follow the Python ones so the two can be read side by side.

static const double STICKY_NM = 0.5;             // epaper.py STICKY_NM
static const int DEPARTURE_MAX_ALT = 4000;       // sources.py DEPARTURE_MAX_ALT
static const uint32_t ORIGIN_MEMORY_MS = 8 * 3600 * 1000UL;  // ORIGIN_MEMORY_SECONDS

// One aircraft from the ADS-B feed.
struct Live {
  char hex[8], cs[12], reg[12], type[8], cat[4], squawk[6];
  double lat, lon, dist;
  bool hasTrack, hasAlt, hasVrate, hasGs, emergency;
  double track, alt, vrate, gs, seen;
};

// Fields kept from the feed (a filter for the streamed JSON parse).
void feedFilter(JsonDocument &filter);

// The nearest airborne aircraft (epaper.py pick). `sticky`: hex on screen now.
struct Pick {
  bool found = false;
  Live ac{};
  int count = 0;   // airborne aircraft with a position (in_range)
};
Pick pick(JsonVariantConst feed, double lat, double lon, const char *sticky);

// Airframe details (sources.py enrich_aircraft): hexdb first, then adsbdb.
struct AcInfo {
  char reg[12] = "", type[40] = "", maker[40] = "";
};
bool parseHexdbAircraft(JsonVariantConst doc, AcInfo &out);
bool parseAdsbdbAircraft(JsonVariantConst doc, AcInfo &out);

// Scheduled route candidates for a callsign (sources.py route_candidates).
struct Route {
  Stub from, to;
};
struct RouteSet {
  int n = 0;
  Route r[2];
};
void addAdsbdbRoute(RouteSet &set, JsonVariantConst doc);
void addHexdbRoute(RouteSet &set, JsonVariantConst doc, const AirportDb &db);

bool routeFits(const Live &ac, const Route &r);
// Nearest sizable airport ahead of a descending aircraft (sources.py _guess_dest).
bool guessDest(const Live &ac, const AirportDb &db, Stub &out);

// Departures the board watched happen (sources.py note_departure), per
// airframe + callsign. Time is passed in so tests can control it.
class DepartureMemory {
 public:
  // Records a takeoff if `ac` is low and climbing near a field; returns the
  // remembered origin for this airframe + callsign, or nullptr.
  const Stub *note(const Live &ac, uint32_t nowMs, const AirportDb &db);
  void clear();

 private:
  struct Entry {
    uint32_t at = 0;   // 0 = empty
    char hex[8] = "", cs[12] = "";
    Stub field;
  };
  Entry entries[48];
};

// What to show, and how far to trust it (epaper.py resolve_route).
struct Resolved {
  bool hasFrom = false, hasTo = false;
  Stub from, to;
  const char *basis = "";   // confirmed | scheduled | track | mismatch
};
Resolved resolveRoute(const Live &ac, const RouteSet &cands, const Stub *seen, const AirportDb &db);

// The response fields (epaper.py nearest()).
void buildFlight(const Pick &p, const AcInfo &info, const Resolved &route, double homeLat, double homeLon,
                 double radiusNm, uint32_t unixNow, Flight &out);
