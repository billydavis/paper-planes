#pragma once
#include <ArduinoJson.h>
#include <stdint.h>

// The nearest aircraft, ready to draw. Same fields as flight-tracker's
// /api/epaper/nearest response (see docs/epaper-api.md); the board now works
// this out itself (route.cpp + src/flights/sources.cpp) and serves it back as JSON at /api/nearest.
// Fixed-size fields so nothing is allocated per poll.
const int32_t NO_VALUE = INT32_MIN;

struct Place {
  char code[8];
  char city[40];
};

struct Flight {
  bool ok = false;           // false = "status: none" (nothing in range)
  uint32_t updated = 0;      // unix seconds
  char hex[8] = "";
  char callsign[12] = "";
  char flight[16] = "";
  bool hasAirline = false;
  char airlineIcao[4] = "";
  char airlineIata[4] = "";
  char airlineName[32] = "";
  char reg[12] = "";
  char typeCode[8] = "";
  char typeName[48] = "";
  char category[4] = "";
  char shape[12] = "";       // narrowbody | widebody | ga | heli
  bool hasRoute = false;
  Place from{}, to{};
  bool hasTo = false;
  char basis[12] = "";       // confirmed | scheduled | track | mismatch
  int32_t altFt = NO_VALUE;
  int32_t vrateFpm = NO_VALUE;
  char vtrend[6] = "level";  // up | down | level
  int32_t gsKt = NO_VALUE;
  int32_t trackDeg = NO_VALUE;
  float distNm = 0;
  int32_t bearingDeg = 0;
  char compass[3] = "";
  char squawk[6] = "";
  bool emergency = false;
  float seenS = -1;
  double lat = 0, lon = 0;
  int32_t inRange = 0;
  float radiusNm = 15;
};

// Parse the endpoint's JSON (the built-in demo samples use it). Returns false
// if it isn't valid.
bool parseFlight(const char *json, Flight &out);

// The reverse: the same JSON shape, for the board's own /api/nearest.
void flightToJson(const Flight &f, JsonDocument &doc);
