#pragma once

// ICAO operator prefix -> airline, from flight-tracker's airlines.js and the
// IATA map in epaper.py (tools/datagen.py builds the table).
struct AirlineRec {
  const char *icao, *iata, *name;
};

// "AAL" -> American, or nullptr if it isn't one we know.
const AirlineRec *findAirline(const char *icao);
