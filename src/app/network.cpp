#include "app/network.h"

#include <WiFi.h>
#include <time.h>

#include "app/constants.h"
#include "app/log.h"
#include "app/settings_store.h"

static bool setupMode = false;

bool clockValid() { return time(nullptr) > (time_t)CLOCK_VALID; }

void networkSetTimeZone(const char *tz) { configTzTime(tz, "pool.ntp.org", "time.nist.gov"); }

bool networkBegin(const Settings &s) {
  WiFi.persistent(false);
  WiFi.setHostname(s.hostname);
  if (s.wifiSsid[0]) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(s.wifiSsid, s.wifiPass);
    LOGI("wifi", "joining %s", s.wifiSsid);
    for (uint32_t t0 = millis(); WiFi.status() != WL_CONNECTED && millis() - t0 < JOIN_MS;) delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    LOGI("wifi", "%s", WiFi.localIP().toString().c_str());
    WiFi.setAutoReconnect(true);
  } else {
    setupMode = true;
    WiFi.mode(s.wifiSsid[0] ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(SETUP_AP_SSID, setupApPassword());
    LOGI("wifi", "setup hotspot \"%s\" (password %s) at %s", SETUP_AP_SSID, setupApPassword(),
         WiFi.softAPIP().toString().c_str());
  }
  networkSetTimeZone(s.tz);
  // Give the clock a moment: lookups are timestamped, night hours need it.
  for (uint32_t t0 = millis(); !setupMode && !clockValid() && millis() - t0 < NTP_WAIT_MS;) delay(100);
  return !setupMode;
}

bool inSetupMode() { return setupMode; }

void networkTickSetup(const Settings &s) {
  static uint32_t lastTry = 0;
  if (!s.wifiSsid[0]) return;
  if (millis() - lastTry > RETRY_MS) {
    lastTry = millis();
    WiFi.begin(s.wifiSsid, s.wifiPass);
  }
  if (WiFi.status() == WL_CONNECTED) {
    LOGI("wifi", "saved network is back, restarting");
    ESP.restart();
  }
}
