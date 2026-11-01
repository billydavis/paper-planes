#pragma once

// Great-circle maths, same formulas as flight-tracker's airports.py.

// Distance in nautical miles.
double gcNm(double aLat, double aLon, double bLat, double bLon);
// Initial bearing from a to b, 0..360 degrees.
double bearingDeg(double aLat, double aLon, double bLat, double bLon);
// |a - b| between two headings, 0..180 degrees.
double angleOff(double a, double b);
// "N", "NE", ... for a bearing (epaper.py compass()).
const char *compass8(double deg);
