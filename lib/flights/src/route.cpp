#include "route.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <algorithm>

#include "airlines.h"
#include "geo.h"
#include "mute.h"
#include "text.h"

static bool isNum(JsonVariantConst v) { return v.is<float>() || v.is<int>(); }

static bool isAirliner(const char *cat) {
  return !strcmp(cat, "A3") || !strcmp(cat, "A4") || !strcmp(cat, "A5");
}

// ---- the feed -----------------------------------------------------------------
void feedFilter(JsonDocument &filter) {
  for (const char *list : {"ac", "aircraft"}) {
    JsonObject f = filter[list][0].to<JsonObject>();
    for (const char *k : {"hex", "flight", "r", "t", "lat", "lon", "alt_baro", "alt_geom", "baro_rate",
                          "geom_rate", "gs", "track", "category", "squawk", "emergency", "seen_pos"})
      f[k] = true;
  }
}

static void readLive(JsonObjectConst a, Live &l) {
  memset(&l, 0, sizeof(l));
  // hex and callsign end up in URL paths (hexdb, adsbdb), so keep only the
  // characters they can legitimately have: a crafted callsign like
  // "X/../alerts?" must not steer those requests. ("~" marks a non-ICAO
  // address in readsb.)
  normalizeHex(a["hex"] | "", l.hex);
  int n = 0;
  for (const char *p = a["flight"] | ""; *p && n < 8; p++)
    if (isalnum((unsigned char)*p)) l.cs[n++] = toupper(*p);
  l.cs[n] = 0;
  scopy(l.reg, a["r"] | "");
  scopy(l.type, a["t"] | "");
  scopy(l.cat, a["category"] | "");
  scopy(l.squawk, a["squawk"] | "");
  l.lat = a["lat"];
  l.lon = a["lon"];
  if ((l.hasTrack = isNum(a["track"]))) l.track = a["track"];
  if (isNum(a["alt_baro"])) { l.hasAlt = true; l.alt = a["alt_baro"]; }
  else if (isNum(a["alt_geom"])) { l.hasAlt = true; l.alt = a["alt_geom"]; }
  if (isNum(a["baro_rate"])) { l.hasVrate = true; l.vrate = a["baro_rate"]; }
  else if (isNum(a["geom_rate"])) { l.hasVrate = true; l.vrate = a["geom_rate"]; }
  if ((l.hasGs = isNum(a["gs"]))) l.gs = a["gs"];
  l.seen = isNum(a["seen_pos"]) ? a["seen_pos"].as<double>() : -1;
  const char *em = a["emergency"] | "none";
  l.emergency = !strcmp(l.squawk, "7500") || !strcmp(l.squawk, "7600") || !strcmp(l.squawk, "7700") ||
                strcmp(em, "none") != 0;
}

Pick pick(JsonVariantConst feed, double lat, double lon, const char *sticky, const MuteList *mutes,
          uint32_t now) {
  JsonArrayConst list = feed["ac"].is<JsonArrayConst>() ? feed["ac"].as<JsonArrayConst>()
                                                         : feed["aircraft"].as<JsonArrayConst>();
  Pick p;
  JsonObjectConst best, cur;
  double bestD = 1e9, curD = 1e9;
  for (JsonObjectConst a : list) {
    if (!isNum(a["lat"]) || !isNum(a["lon"])) continue;
    if (a["alt_baro"].is<const char *>() && !strcmp(a["alt_baro"], "ground")) continue;
    double d = gcNm(lat, lon, a["lat"], a["lon"]);
    p.count++;   // muted aircraft are still in range
    if (mutes && mutes->muted(a["hex"] | "", now)) { p.muted++; continue; }
    if (d < bestD) { bestD = d; best = a; }
    if (sticky && sticky[0] && !strcasecmp(a["hex"] | "", sticky)) { cur = a; curD = d; }
  }
  if (best.isNull()) return p;
  if (!cur.isNull() && curD <= bestD + STICKY_NM) { best = cur; bestD = curD; }
  readLive(best, p.ac);
  p.ac.dist = bestD;
  p.found = true;
  return p;
}

// ---- airframe -------------------------------------------------------------------
bool parseHexdbAircraft(JsonVariantConst doc, AcInfo &out) {
  if (!(doc["Registration"] | "")[0]) return false;
  scopy(out.reg, doc["Registration"] | "");
  scopy(out.type, (doc["Type"] | "")[0] ? doc["Type"] | "" : doc["ICAOTypeCode"] | "");
  scopy(out.maker, doc["Manufacturer"] | "");
  asciiFold(out.type);
  asciiFold(out.maker);
  return true;
}

bool parseAdsbdbAircraft(JsonVariantConst doc, AcInfo &out) {
  JsonObjectConst ac = doc["response"]["aircraft"];
  if (ac.isNull()) return false;
  scopy(out.reg, ac["registration"] | "");
  scopy(out.type, (ac["type"] | "")[0] ? ac["type"] | "" : ac["icao_type"] | "");
  scopy(out.maker, ac["manufacturer"] | "");
  asciiFold(out.type);
  asciiFold(out.maker);
  return true;
}

// ---- routes -----------------------------------------------------------------------
static void addRoute(RouteSet &set, const Stub &from, const Stub &to) {
  if (!from.code[0] || !strcmp(from.code, to.code)) return;   // bogus (CLT -> CLT)
  for (int i = 0; i < set.n; i++)
    if (!strcmp(set.r[i].from.code, from.code) && !strcmp(set.r[i].to.code, to.code)) return;
  if (set.n < 2) set.r[set.n++] = {from, to};
}

static void adsbdbStub(JsonObjectConst p, Stub &out) {
  out = Stub();
  scopy(out.code, (p["iata_code"] | "")[0] ? p["iata_code"] | "" : p["icao_code"] | "");
  scopy(out.city, (p["municipality"] | "")[0] ? p["municipality"] | "" : p["name"] | "");
  asciiFold(out.city);
  if (isNum(p["latitude"]) && isNum(p["longitude"])) {
    out.hasPos = true;
    out.lat = p["latitude"];
    out.lon = p["longitude"];
  }
}

void addAdsbdbRoute(RouteSet &set, JsonVariantConst doc) {
  JsonObjectConst fr = doc["response"]["flightroute"];
  if (!fr["origin"].is<JsonObjectConst>() || !fr["destination"].is<JsonObjectConst>()) return;
  Stub a, b;
  adsbdbStub(fr["origin"], a);
  adsbdbStub(fr["destination"], b);
  addRoute(set, a, b);
}

void addHexdbRoute(RouteSet &set, JsonVariantConst doc, const AirportDb &db) {
  const char *route = doc["route"] | "";
  const char *dash = strchr(route, '-');
  const char *last = strrchr(route, '-');
  if (!dash || dash == route || !last[1]) return;
  char first[8] = "";
  strncpy(first, route, std::min<size_t>(dash - route, sizeof(first) - 1));
  addRoute(set, db.byCode(first), db.byCode(last + 1));
}

bool routeFits(const Live &ac, const Route &r) {
  if (!r.from.hasPos || !r.to.hasPos) return true;
  double dFrom = gcNm(ac.lat, ac.lon, r.from.lat, r.from.lon);
  double dTo = gcNm(ac.lat, ac.lon, r.to.lat, r.to.lon);
  if (dFrom < 40 || dTo < 40) return true;
  if (dFrom + dTo > gcNm(r.from.lat, r.from.lon, r.to.lat, r.to.lon) + 250) return false;
  if (ac.hasTrack && angleOff(ac.track, bearingDeg(ac.lat, ac.lon, r.to.lat, r.to.lon)) > 120) return false;
  return true;
}

// An airliner isn't headed for a small GA field, so only large airports
// count for one.
bool guessDest(const Live &ac, const AirportDb &db, Stub &out) {
  if (!ac.hasAlt || ac.alt >= 12000 || (ac.hasVrate ? ac.vrate : 0) >= -300) return false;
  bool large = isAirliner(ac.cat);
  uint8_t kinds = large ? AP_LARGE : AP_BIG;
  if (ac.alt < 4000 && db.nearest(ac.lat, ac.lon, large ? 35 : 25, kinds, out)) return true;
  if (!ac.hasTrack) return false;
  return db.nearest(ac.lat, ac.lon, large ? 120 : 90, kinds, out, ac.track, 45);
}

// ---- departure memory -----------------------------------------------------------
const Stub *DepartureMemory::note(const Live &ac, uint32_t nowMs, const AirportDb &db) {
  nowMs |= 1;   // 0 marks an empty entry
  auto find = [&]() -> Entry * {
    for (Entry &e : entries)
      if (e.at && nowMs - e.at < ORIGIN_MEMORY_MS && !strcmp(e.hex, ac.hex)) return &e;
    return nullptr;
  };
  if (ac.hasAlt && ac.alt < DEPARTURE_MAX_ALT && ac.hasVrate && ac.vrate > 300) {
    Stub field;
    if (db.nearest(ac.lat, ac.lon, 10, isAirliner(ac.cat) ? AP_LARGE : AP_ANY, field)) {
      Entry *e = find();
      if (!e) {   // free or oldest slot
        e = &entries[0];
        for (Entry &x : entries) {
          if (!x.at) { e = &x; break; }
          if ((int32_t)(x.at - e->at) < 0) e = &x;
        }
      }
      e->at = nowMs;
      scopy(e->hex, ac.hex);
      scopy(e->cs, ac.cs);
      e->field = field;
    }
  }
  Entry *e = find();
  if (e && strcmp(e->cs, ac.cs)) { e->at = 0; return nullptr; }   // new flight number: new leg
  return e ? &e->field : nullptr;
}

void DepartureMemory::clear() {
  for (Entry &e : entries) e = Entry();
}

// ---- resolve_route -------------------------------------------------------------------
Resolved resolveRoute(const Live &ac, const RouteSet &cands, const Stub *seen, const AirportDb &db) {
  Resolved r;
  if (seen) {
    // We know where it took off: a scheduled route that starts there is
    // confirmed; otherwise trust the track.
    r.hasFrom = true;
    r.from = *seen;
    r.basis = "track";
    Stub dest;
    r.hasTo = guessDest(ac, db, dest) && strcmp(dest.code, seen->code);
    if (r.hasTo) r.to = dest;
    for (int i = 0; i < cands.n; i++)
      if (sameField(cands.r[i].from, *seen)) {
        r.from = cands.r[i].from;
        r.to = cands.r[i].to;
        r.hasTo = true;
        r.basis = "confirmed";
        break;
      }
    return r;
  }
  for (int i = 0; i < cands.n; i++)
    if (routeFits(ac, cands.r[i])) {
      r.hasFrom = r.hasTo = true;
      r.from = cands.r[i].from;
      r.to = cands.r[i].to;
      r.basis = "scheduled";
      return r;
    }
  if (cands.n) {
    r.hasFrom = r.hasTo = true;
    r.from = cands.r[0].from;
    r.to = cands.r[0].to;
    r.basis = "mismatch";
  }
  return r;
}

// ---- the response --------------------------------------------------------------------
static const char *shape(const Live &ac) {
  const char *c = ac.cat;
  if (!strcmp(c, "A7")) return "heli";
  if (!strcmp(c, "A5")) return "widebody";
  if (!strcmp(c, "A1") || !strcmp(c, "A2")) return "ga";
  if (!strcmp(c, "A3") || !strcmp(c, "A4") || !strcmp(c, "A6")) return "narrowbody";
  return ac.hasGs && ac.gs >= 250 ? "narrowbody" : "ga";
}

static void place(const Stub &s, Place &p) {
  scopy(p.code, s.code);
  scopy(p.city, s.city);
}

void buildFlight(const Pick &p, const AcInfo &info, const Resolved &route, double homeLat, double homeLon,
                 double radiusNm, uint32_t unixNow, Flight &out) {
  out = Flight();
  out.updated = unixNow;
  out.radiusNm = radiusNm;
  if (!p.found) return;
  const Live &ac = p.ac;
  out.ok = true;
  scopy(out.hex, ac.hex);
  scopy(out.callsign, ac.cs);
  scopy(out.flight, ac.cs);
  // "AAL2417" -> American, "AA 2417".
  if (strlen(ac.cs) >= 4 && isdigit((unsigned char)ac.cs[3]) && isupper((unsigned char)ac.cs[0]) &&
      isupper((unsigned char)ac.cs[1]) && isupper((unsigned char)ac.cs[2])) {
    char icao[4] = {ac.cs[0], ac.cs[1], ac.cs[2], 0};
    if (const AirlineRec *al = findAirline(icao)) {
      out.hasAirline = true;
      scopy(out.airlineIcao, al->icao);
      scopy(out.airlineIata, al->iata);
      scopy(out.airlineName, al->name);
      if (al->iata[0]) snprintf(out.flight, sizeof(out.flight), "%s %s", al->iata, ac.cs + 3);
    }
  }
  // "Boeing 737MAX 8": manufacturer + model, without saying the maker twice.
  const char *model = info.type[0] ? info.type : ac.type;
  if (info.maker[0] && model[0] && strncasecmp(model, info.maker, strlen(info.maker)))
    snprintf(out.typeName, sizeof(out.typeName), "%.20s %.26s", info.maker, model);
  else
    scopy(out.typeName, model);
  scopy(out.reg, info.reg[0] ? info.reg : ac.reg);
  scopy(out.typeCode, ac.type);
  scopy(out.category, ac.cat);
  scopy(out.shape, shape(ac));
  if (route.hasFrom) {
    out.hasRoute = true;
    place(route.from, out.from);
    out.hasTo = route.hasTo;
    if (route.hasTo) place(route.to, out.to);
    scopy(out.basis, route.basis);
  }
  if (ac.hasAlt) out.altFt = std::max(0, (int)ac.alt);
  if (ac.hasVrate) out.vrateFpm = (int)ac.vrate;
  scopy(out.vtrend, !ac.hasVrate || fabs(ac.vrate) < 64 ? "level" : ac.vrate > 0 ? "up" : "down");
  if (ac.hasGs) out.gsKt = lround(ac.gs);
  if (ac.hasTrack) out.trackDeg = lround(ac.track) % 360;
  out.distNm = roundf(ac.dist * 10) / 10;
  double brg = bearingDeg(homeLat, homeLon, ac.lat, ac.lon);
  out.bearingDeg = lround(brg) % 360;
  scopy(out.compass, compass8(brg));
  scopy(out.squawk, ac.squawk);
  out.emergency = ac.emergency;
  out.seenS = ac.seen;
  out.lat = ac.lat;
  out.lon = ac.lon;
  out.inRange = p.count;
}
