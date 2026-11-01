#pragma once
#include "flight.h"
#include "settings.h"

// The nearest airborne aircraft to home, fetched live. The I/O half of
// flight-tracker's /api/epaper/nearest (sources.py): asks the ADS-B feed
// (adsb.lol, then adsb.fi), hexdb/adsbdb for the airframe and the scheduled
// route, caches those, and hands everything to the pure rules in
// lib/flights/src/route.h for the decision.
//
// `sticky` is the hex on screen now: kept unless another aircraft is 0.5 nm+
// closer. Returns false when every ADS-B feed failed (keep the old screen).
bool trackNearest(const Settings &s, const char *sticky, Flight &out);

// How long the last feed request took and whether it reused the connection.
struct FeedTiming {
  uint32_t ms;
  bool reused;
};
FeedTiming lastFeedTiming();
