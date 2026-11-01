// paper-planes: the nearest aircraft on the CrowPanel 5.79" e-paper board.
//
// Stand-alone: the board asks the ADS-B feeds for aircraft near home, works
// out the nearest one and its route itself (a port of flight-tracker's
// /api/epaper/nearest), draws it, and serves the same screen and its
// settings at http://flights.local. See README.md for the layout of the code.
//
//   src/app/      what the board is doing: modes, polling, settings, Wi-Fi
//   src/flights/  live lookups (HTTP + caches), the airport data partition
//   src/net/      HTTPS client
//   src/ui/       canvas, screen layouts, the e-paper panel
//   src/web/      web server, request checks
//   lib/flights/  the route rules and data types (no I/O; unit-tested)
//   lib/settings/ the settings table
//   lib/panel/    canvas <-> panel memory layout

#include <Arduino.h>

#include "app/app.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  appBegin();
}

void loop() { appTick(); }
