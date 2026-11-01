// lib/settings: the settings table, form handling, JSON, night window.
#include <string.h>
#include <unity.h>

#include <map>
#include <string>

#include "settings.h"

void setUp() {}
void tearDown() {}

using Form = std::map<std::string, std::string>;
static const char *get(const char *key, void *ctx) {
  Form &f = *(Form *)ctx;
  auto it = f.find(key);
  return it == f.end() ? nullptr : it->second.c_str();
}
static bool apply(Settings &s, Form f) { return applyForm(s, get, &f); }

static Settings base() {
  Settings s;
  strcpy(s.wifiSsid, "Home");
  strcpy(s.wifiPass, "secret1");
  strcpy(s.tz, "CST6CDT,M3.2.0,M11.1.0");
  strcpy(s.webPass, "admin-pw");
  return s;
}

void test_every_field_has_a_unique_key() {
  for (int i = 0; i < SETTINGS_FIELD_COUNT; i++)
    for (int j = i + 1; j < SETTINGS_FIELD_COUNT; j++)
      TEST_ASSERT_TRUE(strcmp(SETTINGS_FIELDS[i].key, SETTINGS_FIELDS[j].key) != 0);
}

void test_numbers_are_clamped_and_bad_input_ignored() {
  Settings s = base();
  bool restart = apply(s, {{"radius", "500"}, {"poll", "1"}, {"lat", "91.5"}, {"lon", "abc"}, {"night", "on"}});
  TEST_ASSERT_EQUAL(100, s.radiusNm);
  TEST_ASSERT_EQUAL(5, s.pollS);
  TEST_ASSERT_FLOAT_WITHIN(1e-6, 90, s.lat);
  TEST_ASSERT_FLOAT_WITHIN(1e-6, 0, s.lon);
  TEST_ASSERT_FALSE(restart);
}

void test_secrets_blank_keeps_clear_erases() {
  Settings s = base();
  apply(s, {{"ssid", "Home"}, {"pass", ""}, {"webpass", ""}, {"night", "on"}});
  TEST_ASSERT_EQUAL_STRING("secret1", s.wifiPass);
  TEST_ASSERT_EQUAL_STRING("admin-pw", s.webPass);
  apply(s, {{"webpass_clear", "on"}, {"night", "on"}});
  TEST_ASSERT_EQUAL_STRING("", s.webPass);
}

void test_new_network_without_password_is_open_and_restarts() {
  Settings s = base();
  bool restart = apply(s, {{"ssid", "Cafe"}, {"pass", ""}, {"night", "on"}});
  TEST_ASSERT_EQUAL_STRING("Cafe", s.wifiSsid);
  TEST_ASSERT_EQUAL_STRING("", s.wifiPass);
  TEST_ASSERT_TRUE(restart);
}

void test_hostname_is_sanitized() {
  Settings s = base();
  apply(s, {{"host", " My Board!"}, {"night", "on"}});
  TEST_ASSERT_EQUAL_STRING("my-board-", s.hostname);
  apply(s, {{"host", "   "}, {"night", "on"}});
  TEST_ASSERT_EQUAL_STRING("flights", s.hostname);
}

void test_blank_time_zone_keeps_the_old_one() {
  Settings s = base();
  apply(s, {{"tz", " "}, {"night", "on"}});
  TEST_ASSERT_EQUAL_STRING("CST6CDT,M3.2.0,M11.1.0", s.tz);
}

void test_night_times_and_window() {
  Settings s = base();
  apply(s, {{"night", "on"}, {"nstart", "22:30"}, {"nend", "06:15"}});
  TEST_ASSERT_EQUAL(22 * 60 + 30, s.nightStart);
  TEST_ASSERT_EQUAL(6 * 60 + 15, s.nightEnd);
  TEST_ASSERT_TRUE(inNight(s, 23 * 60));
  TEST_ASSERT_TRUE(inNight(s, 3 * 60));
  TEST_ASSERT_FALSE(inNight(s, 12 * 60));
  TEST_ASSERT_FALSE(inNight(s, 6 * 60 + 15));   // end is exclusive
  apply(s, {{"nstart", "25:00"}});                 // invalid: kept; checkbox absent: off
  TEST_ASSERT_EQUAL(22 * 60 + 30, s.nightStart);
  TEST_ASSERT_FALSE(s.nightOn);
  TEST_ASSERT_FALSE(inNight(s, 23 * 60));
  TEST_ASSERT_EQUAL(-1, parseTime("7:5x"));
}

void test_json_never_contains_secrets() {
  Settings s = base();
  JsonDocument doc;
  settingsToJson(s, doc.to<JsonObject>());
  std::string text;
  serializeJson(doc, text);
  TEST_ASSERT_NULL(strstr(text.c_str(), "secret1"));
  TEST_ASSERT_NULL(strstr(text.c_str(), "admin-pw"));
  TEST_ASSERT_TRUE(doc["pass_set"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("22:00", doc["nstart"]);
  TEST_ASSERT_EQUAL_STRING("Home", doc["ssid"]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_field_has_a_unique_key);
  RUN_TEST(test_numbers_are_clamped_and_bad_input_ignored);
  RUN_TEST(test_secrets_blank_keeps_clear_erases);
  RUN_TEST(test_new_network_without_password_is_open_and_restarts);
  RUN_TEST(test_hostname_is_sanitized);
  RUN_TEST(test_blank_time_zone_keeps_the_old_one);
  RUN_TEST(test_night_times_and_window);
  RUN_TEST(test_json_never_contains_secrets);
  return UNITY_END();
}
