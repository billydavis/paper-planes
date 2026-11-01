// lib/flights building blocks: geometry, text folding, the airport image,
// the Flight JSON, and the route rules' edge cases.
#include <math.h>
#include <string.h>
#include <unity.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "airports.h"
#include "flight.h"
#include "geo.h"
#include "route.h"
#include "text.h"

namespace fs = std::filesystem;
static std::string image;

void setUp() {}
void tearDown() {}

// ---- geometry ------------------------------------------------------------------
void test_distance_and_bearing() {
  // ATL -> DFW; reference values from flight-tracker's airports.py (_gc_nm, _bearing).
  TEST_ASSERT_FLOAT_WITHIN(0.1, 634.2, gcNm(33.6367, -84.4281, 32.8968, -97.0380));
  TEST_ASSERT_FLOAT_WITHIN(0.01, 269.47, bearingDeg(33.6367, -84.4281, 32.8968, -97.0380));
  TEST_ASSERT_FLOAT_WITHIN(1e-9, 20, angleOff(350, 10));
  TEST_ASSERT_EQUAL_STRING("N", compass8(359));
  TEST_ASSERT_EQUAL_STRING("SE", compass8(135));
}

// ---- text ------------------------------------------------------------------------
void test_ascii_fold() {
  // (Split literals: a \x escape would otherwise swallow the hex-looking
  // letter after it, as in "\xb3" "d".)
  char s[64] = "Aerom\xc3\xa9" "xico \xc3\x86" "gir \xc5\x81\xc3\xb3" "d\xc5\xba \xe2\x80\x93 S\xc3\xa3" "o Paulo";
  asciiFold(s);
  TEST_ASSERT_EQUAL_STRING("Aeromexico AEgir Lodz - Sao Paulo", s);
}

void test_crc32_known_value() {
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926, crc32((const uint8_t *)"123456789", 9));
}

// ---- airport image ---------------------------------------------------------------
void test_airport_lookup_by_code() {
  TEST_ASSERT_TRUE(airportDb().ready());
  Stub atl = airportDb().byCode("katl");
  TEST_ASSERT_EQUAL_STRING("ATL", atl.code);
  TEST_ASSERT_EQUAL_STRING("Atlanta", atl.city);
  TEST_ASSERT_TRUE(atl.hasPos);
  Stub none = airportDb().byCode("ZZZZ9");
  TEST_ASSERT_FALSE(none.hasPos);
  TEST_ASSERT_EQUAL_STRING("ZZZZ9", none.code);
}

// Regression: the pool used to be emitted as "...\0" "00AK..." in C source,
// where \000 swallowed the digits and shifted every later string.
void test_codes_after_digit_idents_are_intact() {
  for (const char *code : {"KDFW", "KJFK", "EGLL", "KAUS", "00AK"}) {
    Stub s = airportDb().byCode(code);
    TEST_ASSERT_TRUE_MESSAGE(s.hasPos, code);
  }
  TEST_ASSERT_EQUAL_STRING("DFW", airportDb().byCode("KDFW").code);
}

void test_nearest_airport_kinds_and_cone() {
  Stub s;
  TEST_ASSERT_TRUE(airportDb().nearest(30.1945, -97.6699, 10, AP_LARGE, s));
  TEST_ASSERT_EQUAL_STRING("AUS", s.code);
  // 100 nm north of AUS heading south: AUS is ahead; heading north it isn't.
  TEST_ASSERT_TRUE(airportDb().nearest(31.85, -97.67, 120, AP_LARGE, s, 180, 45));
  TEST_ASSERT_TRUE(strcmp(s.code, "AUS") == 0 || strcmp(s.code, "SAT") != 0);
  Stub north;
  bool found = airportDb().nearest(31.85, -97.67, 120, AP_LARGE, north, 0, 45);
  if (found) TEST_ASSERT_TRUE(strcmp(north.code, "AUS") != 0);
}

void test_corrupt_image_is_rejected() {
  std::string bad = image;
  bad[bad.size() / 2] ^= 0x55;
  AirportDb db;
  TEST_ASSERT_FALSE(db.attach((const uint8_t *)bad.data(), bad.size()));
  TEST_ASSERT_FALSE(db.ready());
  TEST_ASSERT_FALSE(db.attach((const uint8_t *)image.data(), 100));   // truncated
  TEST_ASSERT_TRUE(db.attach((const uint8_t *)image.data(), image.size()));
}

// ---- Flight JSON -------------------------------------------------------------------
// Regression: fields written through `doc["key"]` proxies were silently dropped.
void test_flight_json_round_trip() {
  Flight f;
  f.ok = true;
  scopy(f.hex, "a1b2c3");
  scopy(f.callsign, "AAL2770");
  scopy(f.flight, "AA 2770");
  scopy(f.reg, "N915NN");
  scopy(f.typeName, "Boeing 737NG 823/W");
  f.hasRoute = true;
  scopy(f.from.code, "ATL");
  scopy(f.from.city, "Atlanta");
  scopy(f.basis, "mismatch");
  f.altFt = 8225;
  f.gsKt = 280;
  JsonDocument doc;
  flightToJson(f, doc);
  TEST_ASSERT_EQUAL_STRING("AAL2770", doc["callsign"]);
  TEST_ASSERT_EQUAL_STRING("N915NN", doc["reg"]);
  TEST_ASSERT_EQUAL(8225, doc["alt_ft"].as<int>());
  TEST_ASSERT_TRUE(doc["route"]["to"].isNull());
  TEST_ASSERT_TRUE(doc["track_deg"].isNull());   // NO_VALUE -> null
  std::string text;
  serializeJson(doc, text);
  Flight back;
  TEST_ASSERT_TRUE(parseFlight(text.c_str(), back));
  TEST_ASSERT_EQUAL_STRING("AA 2770", back.flight);
  TEST_ASSERT_EQUAL_STRING("Atlanta", back.from.city);
  TEST_ASSERT_FALSE(back.hasTo);
  TEST_ASSERT_EQUAL(NO_VALUE, back.trackDeg);
}

// ---- route rules -------------------------------------------------------------------------
static Live liveAt(double lat, double lon, double alt, double vrate, const char *cs, const char *cat = "A3") {
  Live l;
  memset(&l, 0, sizeof(l));
  scopy(l.hex, "abc123");
  scopy(l.cs, cs);
  scopy(l.cat, cat);
  l.lat = lat;
  l.lon = lon;
  l.hasAlt = true;
  l.alt = alt;
  l.hasVrate = true;
  l.vrate = vrate;
  return l;
}

void test_departure_memory_follows_callsign_and_expires() {
  DepartureMemory m;
  Live climbing = liveAt(33.66, -84.42, 2000, 1500, "DAL1");   // just off ATL
  const Stub *s = m.note(climbing, 1000, airportDb());
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL_STRING("ATL", s->code);
  Live cruising = liveAt(34.5, -84.0, 30000, 0, "DAL1");
  TEST_ASSERT_NOT_NULL(m.note(cruising, 60000, airportDb()));               // remembered
  TEST_ASSERT_NULL(m.note(cruising, 1000 + ORIGIN_MEMORY_MS, airportDb()));   // expired
  m.note(climbing, 2000, airportDb());
  Live renamed = liveAt(34.5, -84.0, 30000, 0, "DAL2");                     // new flight number
  TEST_ASSERT_NULL(m.note(renamed, 3000, airportDb()));
}

void test_hex_and_callsign_are_sanitized_for_urls() {
  JsonDocument feed;
  deserializeJson(feed, R"({"ac":[{"hex":"AB/../c1","flight":"X/../al erts?","lat":1,"lon":1,"alt_baro":100}]})");
  Pick p = pick(feed.as<JsonVariantConst>(), 1, 1, nullptr);
  TEST_ASSERT_TRUE(p.found);
  TEST_ASSERT_EQUAL_STRING("abc1", p.ac.hex);
  TEST_ASSERT_EQUAL_STRING("XALERTS", p.ac.cs);
}

int main() {
  fs::path root = fs::path(__FILE__).parent_path().parent_path().parent_path();
  std::ifstream f(root / "src" / "generated" / "airports.bin", std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  image = ss.str();
  airportDb().attach((const uint8_t *)image.data(), image.size());
  UNITY_BEGIN();
  RUN_TEST(test_distance_and_bearing);
  RUN_TEST(test_ascii_fold);
  RUN_TEST(test_crc32_known_value);
  RUN_TEST(test_airport_lookup_by_code);
  RUN_TEST(test_codes_after_digit_idents_are_intact);
  RUN_TEST(test_nearest_airport_kinds_and_cone);
  RUN_TEST(test_corrupt_image_is_rejected);
  RUN_TEST(test_flight_json_round_trip);
  RUN_TEST(test_departure_memory_follows_callsign_and_expires);
  RUN_TEST(test_hex_and_callsign_are_sanitized_for_urls);
  return UNITY_END();
}
