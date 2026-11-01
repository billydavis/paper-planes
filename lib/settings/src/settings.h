#pragma once
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

// Everything you can change from the web page. One table (SETTINGS_FIELDS)
// describes each setting once; storage, the settings form and the JSON API
// all work from it. To add a setting: a member here, a row in settings.cpp,
// an input in web/settings.html.
struct Settings {
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  char hostname[32] = "flights";   // -> http://flights.local
  double lat = 0, lon = 0;
  int radiusNm = 15;
  char tz[64] = "";                // POSIX TZ string
  int pollS = 10;
  char webPass[33] = "";           // password for settings/update; empty = none
  bool demo = false;               // built-in sample flights, no network lookups
  bool nightOn = true;             // panel off and no lookups during night hours
  int nightStart = 22 * 60;        // minutes after midnight, local time
  int nightEnd = 7 * 60;           // (may wrap past midnight)
};

enum class FieldType : uint8_t {
  Str,      // text
  Secret,   // text never sent back out; blank = keep, "<key>_clear" = erase
  Int,
  Double,
  Bool,     // checkbox: present = on
  Time,     // minutes after midnight, "HH:MM" on the wire
};

enum FieldFlags : uint8_t {
  F_RESTART = 1,    // changing it needs a restart (network settings)
  F_HOSTNAME = 2,   // lower-case letters, digits and '-' only
  F_KEEP_EMPTY = 4, // a blank value keeps the old one
};

struct Field {
  const char *key;      // form field, JSON key and flash (NVS) key
  FieldType type;
  uint16_t offset, size;
  double min, max;      // Int / Double
  uint8_t flags;
};

extern const Field SETTINGS_FIELDS[];
extern const int SETTINGS_FIELD_COUNT;

// Typed access to a field inside a Settings.
inline char *fieldStr(Settings &s, const Field &f) { return (char *)&s + f.offset; }
inline const char *fieldStr(const Settings &s, const Field &f) { return (const char *)&s + f.offset; }
template <typename T> T &fieldRef(Settings &s, const Field &f) { return *(T *)((char *)&s + f.offset); }
template <typename T> const T &fieldRef(const Settings &s, const Field &f) {
  return *(const T *)((const char *)&s + f.offset);
}

// Apply a submitted settings form. `get(key)` returns the submitted value or
// nullptr if the field wasn't sent. Returns true if a restart is needed.
using FormGetter = const char *(*)(const char *key, void *ctx);
bool applyForm(Settings &s, FormGetter get, void *ctx);

// Settings as JSON for the settings page. Secrets come out as "<key>_set": bool.
void settingsToJson(const Settings &s, JsonObject out);

// "HH:MM" <-> minutes after midnight. parse returns -1 if invalid.
int parseTime(const char *hhmm);
void formatTime(int minutes, char *out, size_t n);

// True when `minutes` (local, after midnight) is inside the night window.
bool inNight(const Settings &s, int minutes);
