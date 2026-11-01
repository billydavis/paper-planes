#include "app/settings_store.h"

#include <Preferences.h>
#include <esp_random.h>
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif
#ifndef HOSTNAME
#define HOSTNAME "flights"
#endif
#ifndef HOME_LAT
#define HOME_LAT 0
#endif
#ifndef HOME_LON
#define HOME_LON 0
#endif
#ifndef RADIUS_NM
#define RADIUS_NM 15
#endif
#ifndef TZ_INFO
#define TZ_INFO "EST5EDT,M3.2.0,M11.1.0"
#endif
#ifndef WEB_PASSWORD
#define WEB_PASSWORD ""
#endif
#ifndef SETUP_PASSWORD
#define SETUP_PASSWORD ""
#endif

static const char *NS = "flights";
const char *SETUP_AP_SSID = "flights-setup";
static char setupPass[24] = "";

// First-boot values: struct defaults, then secrets.h.
static Settings defaults() {
  Settings s;
  snprintf(s.wifiSsid, sizeof(s.wifiSsid), "%s", WIFI_SSID);
  snprintf(s.wifiPass, sizeof(s.wifiPass), "%s", WIFI_PASS);
  snprintf(s.hostname, sizeof(s.hostname), "%s", HOSTNAME);
  s.lat = HOME_LAT;
  s.lon = HOME_LON;
  s.radiusNm = RADIUS_NM;
  snprintf(s.tz, sizeof(s.tz), "%s", TZ_INFO);
  snprintf(s.webPass, sizeof(s.webPass), "%s", WEB_PASSWORD);
  return s;
}

// A fixed hotspot password would be the same on every board (and in this
// repo), letting anyone nearby join while the board is in setup mode. So each
// board makes up its own: 10 characters, no look-alikes (0/O, 1/l).
static void makeSetupPassword(Preferences &p) {
  if (strlen(SETUP_PASSWORD) >= 8) {
    snprintf(setupPass, sizeof(setupPass), "%s", SETUP_PASSWORD);
    return;
  }
  String saved = p.getString("setuppw", "");
  if (saved.length() >= 8) {
    snprintf(setupPass, sizeof(setupPass), "%s", saved.c_str());
    return;
  }
  static const char ABC[] = "abcdefghjkmnpqrstuvwxyz23456789";
  for (int i = 0; i < 10; i++) setupPass[i] = ABC[esp_random() % (sizeof(ABC) - 1)];
  setupPass[10] = 0;
  p.putString("setuppw", setupPass);
}

void loadSettings(Settings &s) {
  s = defaults();
  Preferences p;
  p.begin(NS, false);   // read-write: creates the namespace on first boot
  for (int i = 0; i < SETTINGS_FIELD_COUNT; i++) {
    const Field &f = SETTINGS_FIELDS[i];
    if (!p.isKey(f.key)) continue;   // never saved: keep the default
    switch (f.type) {
      case FieldType::Str:
      case FieldType::Secret: {
        String v = p.getString(f.key, "");
        snprintf(fieldStr(s, f), f.size, "%s", v.c_str());
        break;
      }
      case FieldType::Int:
      case FieldType::Time: fieldRef<int>(s, f) = p.getInt(f.key); break;
      case FieldType::Double: fieldRef<double>(s, f) = p.getDouble(f.key); break;
      case FieldType::Bool: fieldRef<bool>(s, f) = p.getBool(f.key); break;
    }
  }
  // Settings from removed features.
  for (const char *old : {"fakey", "falimit"})
    if (p.isKey(old)) p.remove(old);
  makeSetupPassword(p);
  p.end();
  if (!s.hostname[0]) strcpy(s.hostname, "flights");
}

void saveSettings(const Settings &s) {
  Preferences p;
  p.begin(NS, false);
  for (int i = 0; i < SETTINGS_FIELD_COUNT; i++) {
    const Field &f = SETTINGS_FIELDS[i];
    switch (f.type) {
      case FieldType::Str:
      case FieldType::Secret: p.putString(f.key, fieldStr(s, f)); break;
      case FieldType::Int:
      case FieldType::Time: p.putInt(f.key, fieldRef<int>(s, f)); break;
      case FieldType::Double: p.putDouble(f.key, fieldRef<double>(s, f)); break;
      case FieldType::Bool: p.putBool(f.key, fieldRef<bool>(s, f)); break;
    }
  }
  p.end();
}

const char *setupApPassword() { return setupPass; }
