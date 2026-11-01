#include "app/demo.h"

#include <string.h>
#include <time.h>

#include "app/log.h"

static const char *SAMPLES[] = {
  R"({"status":"ok","hex":"a4f2c1","callsign":"AAL2417","flight":"AA 2417",
      "airline":{"icao":"AAL","iata":"AA","name":"American"},"reg":"N421AN",
      "type_name":"Airbus A321neo","shape":"narrowbody",
      "route":{"from":{"code":"DFW","city":"Dallas-Fort Worth"},"to":{"code":"LAX","city":"Los Angeles"},"basis":"confirmed"},
      "alt_ft":8400,"vtrend":"up","gs_kt":286,"track_deg":263,"dist_nm":3.2,"bearing_deg":45,
      "squawk":"4512","emergency":false,"in_range":12,"radius_nm":15})",
  R"({"status":"ok","hex":"a1b2c3","callsign":"N172SP","flight":"N172SP","airline":null,"reg":"N172SP",
      "type_name":"Cessna 172S Skyhawk","shape":"ga",
      "route":{"from":{"code":"GAI","city":"Montgomery County"},"to":null,"basis":"track"},
      "alt_ft":2500,"vtrend":"level","gs_kt":104,"track_deg":18,"dist_nm":1.1,"bearing_deg":200,
      "squawk":"1200","emergency":false,"in_range":5,"radius_nm":15})",
  R"({"status":"ok","hex":"ab12cd","callsign":"UAL88","flight":"UA 88",
      "airline":{"icao":"UAL","iata":"UA","name":"United"},"reg":"N2749U",
      "type_name":"Boeing 777-322ER","shape":"widebody",
      "route":{"from":{"code":"KEWR","city":"Newark"},"to":{"code":"RJAA","city":"Tokyo Narita International"},"basis":"mismatch"},
      "alt_ft":36000,"vtrend":"down","gs_kt":512,"track_deg":312,"dist_nm":12.6,"bearing_deg":300,
      "squawk":"7700","emergency":true,"in_range":20,"radius_nm":15})",
  R"({"status":"ok","hex":"a9e0f1","callsign":"N911LF","flight":"N911LF","airline":null,"reg":"N911LF",
      "type_name":"Airbus Helicopters EC135","shape":"heli","route":null,
      "alt_ft":1100,"vtrend":"up","gs_kt":118,"track_deg":95,"dist_nm":0.8,"bearing_deg":120,
      "squawk":"0322","emergency":false,"in_range":3,"radius_nm":15})",
  R"({"status":"none","radius_nm":15})",
};
static const int NSAMPLES = sizeof(SAMPLES) / sizeof(SAMPLES[0]);

Flight demoNext() {
  static int sample = -1;
  sample = (sample + 1) % NSAMPLES;
  Flight f;
  if (parseFlight(SAMPLES[sample], f)) f.updated = time(nullptr);
  LOGI("demo", "sample %d", sample);
  return f;
}

void demoNudge(Flight &f) {
  if (!f.ok) return;
  if (f.altFt != NO_VALUE && strcmp(f.vtrend, "level")) f.altFt += !strcmp(f.vtrend, "down") ? -300 : 300;
  if (f.distNm > 0.3f) f.distNm -= 0.2f;
}
