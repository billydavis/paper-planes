#include "flight.h"

#include <ArduinoJson.h>
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
    COPY(f.airlineName, al["name"]);
  }
  COPY(f.reg, doc["reg"]);
  COPY(f.typeName, doc["type_name"]);
  if (!f.typeName[0]) COPY(f.typeName, doc["type_code"]);
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
  COPY(f.vtrend, doc["vtrend"]);
  f.gsKt = num(doc["gs_kt"]);
  f.trackDeg = num(doc["track_deg"]);
  f.distNm = doc["dist_nm"] | 0.0f;
  f.bearingDeg = doc["bearing_deg"] | 0;
  COPY(f.squawk, doc["squawk"]);
  f.emergency = doc["emergency"] | false;
  f.inRange = doc["in_range"] | 0;
  return true;
}
