#include "logo.h"

#include <string.h>

#include "generated/logos_gen.h"

const Logo *findLogo(const char *icao) {
  if (!icao || strlen(icao) != 3) return nullptr;
  for (int i = 0; i < NLOGOS; i++)
    if (!strcmp(LOGOS[i].icao, icao)) return &LOGOS[i];
  return nullptr;
}
