#include "airlines.h"

#include <string.h>

#include "generated/airlines_gen.h"

const AirlineRec *findAirline(const char *icao) {
  int a = 0, b = AIRLINE_COUNT - 1;
  while (a <= b) {
    int m = (a + b) / 2;
    int c = strcmp(AIRLINES[m].icao, icao);
    if (c == 0) return &AIRLINES[m];
    if (c < 0) a = m + 1; else b = m - 1;
  }
  return nullptr;
}
