#pragma once
#include <stddef.h>
#include <stdint.h>

// The airport table lives in its own flash partition ("airports" in
// partitions.csv), written by `pio run -t upload` (tools/pio_generate.py adds
// it) or uploaded at http://flights.local/update. Keeping it out of the
// firmware makes firmware updates ~1.5 MB smaller.

// Map the partition and hand it to airportDb(). False if it's missing or
// fails its checks (the board still runs; routes just lack airport lookups).
bool airportDataBegin();

// For an over-the-air data upload: stop using the partition before it's
// overwritten. (The board restarts afterwards and maps it again.)
void airportDataRelease();

// Short status for the web page: "47753 airports" / "missing" / "updating".
const char *airportDataStatus();
