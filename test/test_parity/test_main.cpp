// Parity with flight-tracker: every fixture in test/fixtures holds the upstream
// replies flight-tracker's epaper.nearest() got and the answer it gave (made by
// tools/make_fixtures.py). Feed the same replies through lib/flights the way
// src/flights/sources.cpp does, and expect the same answer.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "flight.h"
#include "route.h"

namespace fs = std::filesystem;

static fs::path projectDir() { return fs::path(__FILE__).parent_path().parent_path().parent_path(); }

static std::string readFile(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static std::string airportImage;   // kept alive: AirportDb points into it

void setUp() {}
void tearDown() {}

// What src/flights/sources.cpp trackNearest() does, minus the HTTP.
static void runFixture(JsonDocument &fx, Flight &out) {
  double lat = fx["home"]["lat"], lon = fx["home"]["lon"], radius = fx["home"]["radius"];
  const char *sticky = fx["sticky"] | "";
  JsonVariantConst replies = fx["replies"];
  Pick p = pick(replies["feed"], lat, lon, sticky);
  AcInfo info;
  Resolved route;
  if (p.found) {
    if (!(replies["hexdb_aircraft"].is<JsonObjectConst>() && parseHexdbAircraft(replies["hexdb_aircraft"], info)))
      if (replies["adsbdb_aircraft"].is<JsonObjectConst>()) parseAdsbdbAircraft(replies["adsbdb_aircraft"], info);
    RouteSet cands;
    if (p.ac.cs[0]) {
      if (replies["adsbdb_callsign"].is<JsonObjectConst>()) addAdsbdbRoute(cands, replies["adsbdb_callsign"]);
      if (replies["hexdb_route"].is<JsonObjectConst>()) addHexdbRoute(cands, replies["hexdb_route"], airportDb());
    }
    DepartureMemory memory;
    const Stub *seen = memory.note(p.ac, 1000, airportDb());
    route = resolveRoute(p.ac, cands, seen, airportDb());
  }
  buildFlight(p, info, route, lat, lon, radius, 0, out);
}

// Compare one field; collects a readable list of differences.
struct Diff {
  std::string text;
  void str(const char *key, JsonVariantConst want, JsonVariantConst got) {
    std::string w = want.isNull() ? "null" : want.as<std::string>();
    std::string g = got.isNull() ? "null" : got.as<std::string>();
    if (w != g) text += std::string(key) + ": want " + w + ", got " + g + "\n";
  }
  void num(const char *key, JsonVariantConst want, JsonVariantConst got, double tol) {
    if (want.isNull() != got.isNull() || (!want.isNull() && fabs(want.as<double>() - got.as<double>()) > tol)) {
      char b[160];
      snprintf(b, sizeof(b), "%s: want %s, got %s\n", key, want.isNull() ? "null" : want.as<std::string>().c_str(),
               got.isNull() ? "null" : got.as<std::string>().c_str());
      text += b;
    }
  }
};

static void compare(JsonVariantConst want, JsonVariantConst got, Diff &d) {
  d.str("status", want["status"], got["status"]);
  if (want["status"] != "ok") return;
  for (const char *k : {"hex", "callsign", "flight", "reg", "type_code", "type_name", "shape", "vtrend",
                        "compass", "squawk"})
    d.str(k, want[k], got[k]);
  d.str("airline.icao", want["airline"]["icao"], got["airline"]["icao"]);
  d.str("airline.name", want["airline"]["name"], got["airline"]["name"]);
  d.str("emergency", want["emergency"], got["emergency"]);
  if (want["route"].isNull() != got["route"].isNull()) {
    d.text += std::string("route: want ") + (want["route"].isNull() ? "none" : "one") + ", got " +
              (got["route"].isNull() ? "none" : "one") + "\n";
  } else if (!want["route"].isNull()) {
    d.str("route.basis", want["route"]["basis"], got["route"]["basis"]);
    d.str("route.from.code", want["route"]["from"]["code"], got["route"]["from"]["code"]);
    d.str("route.from.city", want["route"]["from"]["city"], got["route"]["from"]["city"]);
    d.str("route.to.code", want["route"]["to"]["code"], got["route"]["to"]["code"]);
    d.str("route.to.city", want["route"]["to"]["city"], got["route"]["to"]["city"]);
  }
  d.num("alt_ft", want["alt_ft"], got["alt_ft"], 0);
  d.num("vrate_fpm", want["vrate_fpm"], got["vrate_fpm"], 0);
  d.num("gs_kt", want["gs_kt"], got["gs_kt"], 1);          // Python rounds half to even
  d.num("track_deg", want["track_deg"], got["track_deg"], 1);
  d.num("bearing_deg", want["bearing_deg"], got["bearing_deg"], 1);
  d.num("dist_nm", want["dist_nm"], got["dist_nm"], 0.1001);
  d.num("in_range", want["in_range"], got["in_range"], 0);
}

void test_every_fixture_matches_flight_tracker() {
  fs::path dir = projectDir() / "test" / "fixtures";
  std::vector<fs::path> files;
  for (auto &e : fs::directory_iterator(dir))
    if (e.path().extension() == ".json") files.push_back(e.path());
  TEST_ASSERT_TRUE_MESSAGE(files.size() > 0, "no fixtures: run tools/make_fixtures.py");
  std::string failures;
  for (auto &path : files) {
    JsonDocument fx;
    std::string text = readFile(path);
    TEST_ASSERT_FALSE_MESSAGE(deserializeJson(fx, text), path.string().c_str());
    Flight f;
    runFixture(fx, f);
    JsonDocument got;
    flightToJson(f, got);
    Diff d;
    compare(fx["expected"], got.as<JsonVariantConst>(), d);
    if (!d.text.empty()) failures += path.filename().string() + ":\n" + d.text;
    else printf("  %-30s ok\n", path.filename().string().c_str());
  }
  TEST_ASSERT_TRUE_MESSAGE(failures.empty(), failures.c_str());
}

int main() {
  airportImage = readFile(projectDir() / "src" / "generated" / "airports.bin");
  if (!airportDb().attach((const uint8_t *)airportImage.data(), airportImage.size()))
    printf("airports.bin missing or bad: run tools/datagen.py\n");
  UNITY_BEGIN();
  RUN_TEST(test_every_fixture_matches_flight_tracker);
  return UNITY_END();
}
