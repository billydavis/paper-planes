#pragma once
#include <stdint.h>

// Timing and cache constants for the board, in one place. (The route rules'
// own constants live with them in lib/flights/src/route.h, matching the
// Python they mirror.)

// Screen
static const uint32_t FULL_EVERY_MS = 30 * 60 * 1000UL;  // forced clean redraw this often
static const uint32_t STALE_MS = 120000;                  // mark the screen stale after this without data
static const uint32_t DEMO_MS = 45000;                    // next demo flight this often

// Network
static const uint32_t JOIN_MS = 20000;     // give up joining Wi-Fi at boot after this, open the hotspot
static const uint32_t RETRY_MS = 60000;    // in setup mode, try the saved network again this often
static const uint32_t NTP_WAIT_MS = 8000;  // wait this long for the clock at boot
static const uint32_t HTTP_TIMEOUT_MS = 8000;

// Upstream caches (epaper.py AIRCRAFT_TTL / ROUTE_TTL)
static const uint32_t AIRCRAFT_TTL_MS = 24 * 3600 * 1000UL;
static const uint32_t ROUTE_TTL_MS = 30 * 60 * 1000UL;

// A clock reading before this is "not set yet" (unix seconds, Nov 2023).
static const uint32_t CLOCK_VALID = 1700000000UL;
