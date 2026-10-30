#pragma once
#include <stdint.h>

// One /api/epaper/nearest response (see docs/epaper-api.md), in fixed-size
// fields so nothing is allocated per poll.
const int32_t NO_VALUE = INT32_MIN;

struct Place {
  char code[8];
  char city[40];
};

struct Flight {
  bool ok = false;           // false = "status: none" (nothing in range)
  char hex[8] = "";
  char callsign[12] = "";
  char flight[16] = "";
  bool hasAirline = false;
  char airlineIcao[4] = "";
  char airlineName[32] = "";
  char reg[12] = "";
  char typeName[48] = "";
  char shape[12] = "";       // narrowbody | widebody | ga | heli
  bool hasRoute = false;
  Place from{}, to{};
  bool hasTo = false;
  char basis[12] = "";       // confirmed | scheduled | track | mismatch
  int32_t altFt = NO_VALUE;
  char vtrend[6] = "level";  // up | down | level
  int32_t gsKt = NO_VALUE;
  int32_t trackDeg = NO_VALUE;
  float distNm = 0;
  int32_t bearingDeg = 0;
  char squawk[6] = "";
  bool emergency = false;
  int32_t inRange = 0;
  float radiusNm = 15;
};

// Parse the endpoint's JSON. Returns false if it isn't valid.
bool parseFlight(const char *json, Flight &out);
