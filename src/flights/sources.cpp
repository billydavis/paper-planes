#include "flights/sources.h"

#include <Arduino.h>
#include <time.h>

#include "app/constants.h"
#include "app/log.h"
#include "app/mutes.h"
#include "net/https.h"
#include "route.h"
#include "text.h"

static Https feed(true);      // polled every few seconds: keep the connection
static Https lookups(false);  // hexdb/adsbdb, once per new aircraft
static DepartureMemory departures;

// Small fixed caches; the oldest entry is replaced when full.
template <typename T, size_t N>
struct Cache {
  struct Entry {
    uint32_t at = 0;   // millis | 1; 0 = empty
    char key[12] = "";
    T value;
  };
  Entry items[N];

  T *find(const char *key, uint32_t ttl) {
    for (Entry &e : items)
      if (e.at && millis() - e.at < ttl && !strcmp(e.key, key)) return &e.value;
    return nullptr;
  }
  T &insert(const char *key) {
    Entry *slot = &items[0];
    for (Entry &e : items) {
      if (!e.at) { slot = &e; break; }
      if ((int32_t)(e.at - slot->at) < 0) slot = &e;
    }
    slot->at = millis() | 1;
    scopy(slot->key, key);
    slot->value = T();
    return slot->value;
  }
};
static Cache<AcInfo, 24> acCache;
static Cache<RouteSet, 16> routeCache;

// Airframe: hexdb, then adsbdb (sources.py enrich_aircraft).
static const AcInfo &aircraftInfo(const char *hex) {
  if (AcInfo *hit = acCache.find(hex, AIRCRAFT_TTL_MS)) return *hit;
  AcInfo &info = acCache.insert(hex);
  JsonDocument doc;
  char url[96];
  snprintf(url, sizeof(url), "https://hexdb.io/api/v1/aircraft/%s", hex);
  if (lookups.getJson(url, doc) == 200 && parseHexdbAircraft(doc.as<JsonVariantConst>(), info)) return info;
  snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/aircraft/%s", hex);
  if (lookups.getJson(url, doc) == 200) parseAdsbdbAircraft(doc.as<JsonVariantConst>(), info);
  return info;
}

// Scheduled routes for a callsign: adsbdb, then hexdb (sources.py route_candidates).
static const RouteSet &routeCandidates(const char *cs) {
  static const RouteSet none;
  if (!cs[0]) return none;
  if (RouteSet *hit = routeCache.find(cs, ROUTE_TTL_MS)) return *hit;
  RouteSet &set = routeCache.insert(cs);
  JsonDocument doc;
  char url[96];
  snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", cs);
  if (lookups.getJson(url, doc) == 200) addAdsbdbRoute(set, doc.as<JsonVariantConst>());
  snprintf(url, sizeof(url), "https://hexdb.io/api/v1/route/icao/%s", cs);
  if (lookups.getJson(url, doc) == 200) addHexdbRoute(set, doc.as<JsonVariantConst>(), airportDb());
  return set;
}

// Live aircraft near home. False if every provider failed.
static bool nearby(const Settings &s, JsonDocument &doc) {
  static JsonDocument filter;
  if (filter.isNull()) feedFilter(filter);
  char url[160];
  snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.5f/%.5f/%d", s.lat, s.lon, s.radiusNm);
  if (feed.getJson(url, doc, &filter) == 200) return true;
  snprintf(url, sizeof(url), "https://opendata.adsb.fi/api/v2/lat/%.5f/lon/%.5f/dist/%d", s.lat, s.lon,
           s.radiusNm);
  return feed.getJson(url, doc, &filter) == 200;
}

bool trackNearest(const Settings &s, const char *sticky, Flight &out) {
  double radius = constrain(s.radiusNm, 1, 100);
  JsonDocument doc;
  if (!nearby(s, doc)) return false;
  MuteList mutes = mutesSnapshot();
  Pick p = pick(doc.as<JsonVariantConst>(), s.lat, s.lon, sticky, &mutes, mutesNow());
  doc.clear();

  static const AcInfo noInfo;
  const AcInfo &info = p.found ? aircraftInfo(p.ac.hex) : noInfo;
  Resolved route;
  if (p.found) {
    const RouteSet &cands = routeCandidates(p.ac.cs);
    const Stub *seen = departures.note(p.ac, millis(), airportDb());
    route = resolveRoute(p.ac, cands, seen, airportDb());
  }
  time_t now = time(nullptr);
  buildFlight(p, info, route, s.lat, s.lon, radius, now > (time_t)CLOCK_VALID ? now : 0, out);
  return true;
}

FeedTiming lastFeedTiming() { return {feed.lastMs(), feed.lastReused()}; }
