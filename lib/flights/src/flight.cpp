#include "flight.h"

#include <math.h>
#include <string.h>

static void copy(char *dst, size_t n, JsonVariantConst v) {
  const char *s = v.is<const char *>() ? v.as<const char *>() : "";
  strncpy(dst, s ? s : "", n - 1);
  dst[n - 1] = 0;
}
#define COPY(dst, v) copy(dst, sizeof(dst), v)

static int32_t num(JsonVariantConst v) {
  return v.is<float>() || v.is<int>() ? (int32_t)v.as<float>() : NO_VALUE;
}

bool parseFlight(const char *json, Flight &f) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  const char *status = doc["status"] | "";
  f = Flight();
  f.radiusNm = doc["radius_nm"] | 15.0f;
  f.updated = doc["updated"] | 0u;
  if (strcmp(status, "none") == 0) return true;
  if (strcmp(status, "ok") != 0) return false;

  f.ok = true;
  COPY(f.hex, doc["hex"]);
  COPY(f.callsign, doc["callsign"]);
  COPY(f.flight, doc["flight"]);
  JsonVariantConst al = doc["airline"];
  if (al.is<JsonObjectConst>()) {
    f.hasAirline = true;
    COPY(f.airlineIcao, al["icao"]);
    COPY(f.airlineIata, al["iata"]);
    COPY(f.airlineName, al["name"]);
  }
  COPY(f.reg, doc["reg"]);
  COPY(f.typeCode, doc["type_code"]);
  COPY(f.typeName, doc["type_name"]);
  if (!f.typeName[0]) COPY(f.typeName, doc["type_code"]);
  COPY(f.category, doc["category"]);
  COPY(f.shape, doc["shape"]);
  JsonVariantConst rt = doc["route"];
  if (rt.is<JsonObjectConst>() && rt["from"].is<JsonObjectConst>()) {
    f.hasRoute = true;
    COPY(f.from.code, rt["from"]["code"]);
    COPY(f.from.city, rt["from"]["city"]);
    if (rt["to"].is<JsonObjectConst>()) {
      f.hasTo = true;
      COPY(f.to.code, rt["to"]["code"]);
      COPY(f.to.city, rt["to"]["city"]);
    }
    COPY(f.basis, rt["basis"]);
  }
  f.altFt = num(doc["alt_ft"]);
  f.vrateFpm = num(doc["vrate_fpm"]);
  COPY(f.vtrend, doc["vtrend"]);
  f.gsKt = num(doc["gs_kt"]);
  f.trackDeg = num(doc["track_deg"]);
  f.distNm = doc["dist_nm"] | 0.0f;
  f.bearingDeg = doc["bearing_deg"] | 0;
  COPY(f.compass, doc["compass"]);
  COPY(f.squawk, doc["squawk"]);
  f.emergency = doc["emergency"] | false;
  f.seenS = doc["seen_s"] | -1.0f;
  f.lat = doc["lat"] | 0.0;
  f.lon = doc["lon"] | 0.0;
  f.inRange = doc["in_range"] | 0;
  return true;
}

// Empty strings and NO_VALUE go out as null, like the server sent them.
// (Templates so `doc["key"]` proxies get the member created on write.)
template <typename V> static void put(V &&v, const char *s) { if (s[0]) v.set(s); else v.set(nullptr); }
template <typename V> static void put(V &&v, int32_t n) { if (n != NO_VALUE) v.set(n); else v.set(nullptr); }

void flightToJson(const Flight &f, JsonDocument &doc) {
  doc.clear();
  doc["status"] = f.ok ? "ok" : "none";
  doc["updated"] = f.updated;
  doc["radius_nm"] = f.radiusNm;
  if (!f.ok) return;
  doc["hex"] = f.hex;
  put(doc["callsign"], f.callsign);
  put(doc["flight"], f.flight);
  if (f.hasAirline) {
    JsonObject al = doc["airline"].to<JsonObject>();
    al["icao"] = f.airlineIcao;
    put(al["iata"], f.airlineIata);
    al["name"] = f.airlineName;
  } else {
    doc["airline"] = nullptr;
  }
  put(doc["reg"], f.reg);
  put(doc["type_code"], f.typeCode);
  put(doc["type_name"], f.typeName);
  put(doc["category"], f.category);
  doc["shape"] = f.shape;
  if (f.hasRoute) {
    JsonObject rt = doc["route"].to<JsonObject>();
    rt["from"]["code"] = f.from.code;
    rt["from"]["city"] = f.from.city;
    if (f.hasTo) {
      rt["to"]["code"] = f.to.code;
      rt["to"]["city"] = f.to.city;
    } else {
      rt["to"] = nullptr;
    }
    rt["basis"] = f.basis;
  } else {
    doc["route"] = nullptr;
  }
  put(doc["alt_ft"], f.altFt);
  put(doc["vrate_fpm"], f.vrateFpm);
  doc["vtrend"] = f.vtrend;
  put(doc["gs_kt"], f.gsKt);
  put(doc["track_deg"], f.trackDeg);
  doc["dist_nm"] = roundf(f.distNm * 10) / 10;
  doc["bearing_deg"] = f.bearingDeg;
  put(doc["compass"], f.compass);
  put(doc["squawk"], f.squawk);
  doc["emergency"] = f.emergency;
  if (f.seenS >= 0) doc["seen_s"] = f.seenS; else doc["seen_s"] = nullptr;
  if (f.lat || f.lon) { doc["lat"] = f.lat; doc["lon"] = f.lon; }
  doc["in_range"] = f.inRange;
}
