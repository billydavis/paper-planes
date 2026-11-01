#pragma once
#include "flight.h"
#include "settings.h"
#include "ui/display.h"

// The board's own web server, at http://<hostname>.local (mDNS):
//   /              what's on the screen right now (live picture + details)
//   /settings      settings page (web/settings.html)
//   /update        firmware or airport-data upload (web/update.html)
//   /api/nearest   the current flight as JSON (same shape as flight-tracker's
//                  /api/epaper/nearest)
//   /api/state     that plus device status, for the landing page
//   /api/settings  the settings (password needed when one is set)
//   /screen.bmp    the panel's current frame
// Runs in its own task, so it answers while the board is busy fetching or
// refreshing the panel. Request checks: web/guard.h.

// `setupMode` = the board is running its own hotspot: every unknown URL then
// redirects to the settings page (captive portal).
void webBegin(const Settings &s, bool setupMode, const Display &display);

// Hand the web page the latest data and status line.
void webPublish(const Flight &f, bool haveData, const char *status);

// True once if the settings were changed from the web page; `out` gets them.
bool webSettingsChanged(Settings &out);
