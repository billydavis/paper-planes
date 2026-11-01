#pragma once
#include "ui/canvas.h"
#include "flight.h"

// Screen layouts. Coordinates follow tools/mockup.py so the panel matches the
// mockups in docs/mockups/.

// The nearest-aircraft card. `caption` goes under the logo (normally the
// callsign; a stale-data warning when updates have stopped).
void drawFlight(Canvas &c, const Flight &f, const char *caption);

// Big title + two lines of detail, for "quiet skies", "no Wi-Fi" and so on.
void drawMessage(Canvas &c, const char *title, const char *line1, const char *line2,
                 const char *footer);
