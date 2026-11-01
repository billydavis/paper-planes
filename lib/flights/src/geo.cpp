#include "geo.h"

#include <math.h>

static const double DEG = M_PI / 180;

double gcNm(double aLat, double aLon, double bLat, double bLon) {
  double sLat = sin((bLat - aLat) * DEG / 2), sLon = sin((bLon - aLon) * DEG / 2);
  double s = sLat * sLat + cos(aLat * DEG) * cos(bLat * DEG) * sLon * sLon;
  return 3440.065 * 2 * atan2(sqrt(s), sqrt(1 - s));
}

double bearingDeg(double aLat, double aLon, double bLat, double bLon) {
  double dlon = (bLon - aLon) * DEG;
  double y = sin(dlon) * cos(bLat * DEG);
  double x = cos(aLat * DEG) * sin(bLat * DEG) - sin(aLat * DEG) * cos(bLat * DEG) * cos(dlon);
  return fmod(atan2(y, x) / DEG + 360, 360);
}

double angleOff(double a, double b) { return fabs(fmod(a - b + 540, 360) - 180); }

const char *compass8(double deg) {
  static const char *NAMES[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return NAMES[(int)lround(deg / 45) % 8];
}
