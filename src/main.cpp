// Nearest-flight display for the CrowPanel 5.79" e-paper board.
//
// Polls the flight-tracker server's /api/epaper/nearest every POLL_MS and
// draws the closest airborne aircraft. Full refresh when the aircraft changes,
// partial refresh when only its numbers change.
//
// Serial commands (115200 baud):
//   d  dump the current screen as hex (tools/grab_screen.py turns it into a PNG)
//   f  force a full refresh
//   n  next sample flight (demo mode only)

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

#include "display.h"
#include "flight.h"
#include "screen.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

static const uint32_t POLL_MS = 10000;
static const uint32_t DEMO_MS = 45000;
static const uint32_t STALE_MS = 120000;   // mark the screen stale after this long without data
static const uint32_t FULL_EVERY_MS = 30 * 60 * 1000UL;

static Display display;
static Canvas canvas;
static Flight flight;          // last good response
static bool haveData = false;
static uint32_t lastGood = 0, lastPoll = 0, lastFull = 0;
static char shownHex[8] = "";
static bool shownMessage = true;
static char lastSeen[48] = ""; // "AA 2417 at 14:32", for the quiet-skies screen
static bool demo = sizeof(WIFI_SSID) <= 1;

// ---- demo samples (same shape as the endpoint's JSON) -------------------------
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
static int sample = -1;

// ---- helpers -----------------------------------------------------------------
static bool clockText(char *out, size_t n, const char *fmt) {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  strftime(out, n, fmt, &t);
  return true;
}

static void render(bool forceFull) {
  char caption[32];
  bool stale = haveData && !demo && millis() - lastGood > STALE_MS;
  if (stale) snprintf(caption, sizeof(caption), "NO UPDATE %lu MIN", (millis() - lastGood) / 60000);

  bool isMessage;
  bool full = forceFull || millis() - lastFull > FULL_EVERY_MS;
  if (haveData && flight.ok) {
    if (!stale) snprintf(caption, sizeof(caption), "%s", flight.callsign[0] ? flight.callsign : flight.hex);
    drawFlight(canvas, flight, caption);
    isMessage = false;
    if (shownMessage || strcmp(shownHex, flight.hex)) full = true;   // new aircraft
    strcpy(shownHex, flight.hex);
  } else {
    char l1[48], l2[64], foot[40] = "";
    if (haveData) {
      snprintf(l1, sizeof(l1), "Nothing airborne within %.0f nm", flight.radiusNm);
      if (lastSeen[0]) snprintf(l2, sizeof(l2), "Last seen: %s", lastSeen);
      else l2[0] = 0;
      char hm[8];
      if (clockText(hm, sizeof(hm), "%H:%M")) snprintf(foot, sizeof(foot), "UPDATED %s", hm);
      if (stale) snprintf(foot, sizeof(foot), "%s", caption);
      drawMessage(canvas, "QUIET SKIES", l1, l2, foot);
    } else if (!demo && WiFi.status() != WL_CONNECTED) {
      drawMessage(canvas, "NO WI-FI", "Trying to join " WIFI_SSID, "Check src/secrets.h", nullptr);
    } else if (!demo) {
      drawMessage(canvas, "NO SERVER", "Can't reach the flight server", SERVER_URL, nullptr);
    } else {
      drawMessage(canvas, "STARTING", "Demo mode", nullptr, nullptr);
    }
    if (!shownMessage) full = true;
    isMessage = true;
    shownHex[0] = 0;
  }
  if (display.show(canvas, full) && full) lastFull = millis();
  shownMessage = isMessage;
}

static void accept(const Flight &f) {
  if (f.ok) {
    char hm[8] = "";
    clockText(hm, sizeof(hm), "%H:%M");
    snprintf(lastSeen, sizeof(lastSeen), "%s%s%s", f.flight[0] ? f.flight : f.reg, hm[0] ? " at " : "", hm);
  }
  flight = f;
  haveData = true;
  lastGood = millis();
}

static void poll() {
  if (WiFi.status() != WL_CONNECTED) return;
  char url[256];
  snprintf(url, sizeof(url), "%s/api/epaper/nearest?lat=%.5f&lon=%.5f&radius=%d%s%s", SERVER_URL,
           (double)HOME_LAT, (double)HOME_LON, (int)RADIUS_NM,
           haveData && flight.ok ? "&sticky=" : "", haveData && flight.ok ? flight.hex : "");
  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(10000);
  if (!http.begin(url)) return;
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    Flight f;
    if (parseFlight(body.c_str(), f)) accept(f);
    else Serial.println("[poll] bad JSON");
  } else {
    Serial.printf("[poll] HTTP %d\n", code);
  }
  http.end();
}

static void dumpScreen() {
  const Canvas &c = display.current();
  Serial.printf("BEGIN CANVAS %d %d\n", Canvas::W, Canvas::H);
  static const char *hex = "0123456789abcdef";
  char line[Canvas::STRIDE * 2 + 1];
  for (int y = 0; y < Canvas::H; y++) {
    for (int i = 0; i < Canvas::STRIDE; i++) {
      uint8_t b = c.buf[y * Canvas::STRIDE + i];
      line[2 * i] = hex[b >> 4];
      line[2 * i + 1] = hex[b & 15];
    }
    line[Canvas::STRIDE * 2] = 0;
    Serial.println(line);
  }
  Serial.println("END CANVAS");
}

static void nextSample() {
  sample = (sample + 1) % NSAMPLES;
  Flight f;
  if (parseFlight(SAMPLES[sample], f)) accept(f);
  Serial.printf("[demo] sample %d\n", sample);
}

// ---- Arduino entry points ------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\nnearest-flight starting (%s)\n", demo ? "demo mode" : "Wi-Fi " WIFI_SSID);
  display.begin();
  if (demo) {
    nextSample();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) delay(250);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[wifi] %s\n", WiFi.localIP().toString().c_str());
      poll();
      lastPoll = millis();
    }
  }
  render(true);
}

void loop() {
  bool force = false;
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == 'd') dumpScreen();
    if (ch == 'f') force = true;
    if (ch == 'n' && demo) { nextSample(); lastPoll = millis(); force = true; }
  }

  uint32_t now = millis();
  if (demo) {
    if (now - lastPoll >= DEMO_MS) { nextSample(); lastPoll = now; render(force); return; }
    static uint32_t lastTick = 0;
    if (flight.ok && now - lastTick >= POLL_MS) {   // nudge the numbers: exercises partial refresh
      lastTick = now;
      if (flight.altFt != NO_VALUE && strcmp(flight.vtrend, "level"))
        flight.altFt += !strcmp(flight.vtrend, "down") ? -300 : 300;
      flight.distNm = flight.distNm > 0.3f ? flight.distNm - 0.2f : flight.distNm;
      render(force);
      return;
    }
  } else if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    poll();
    render(force);
    return;
  }
  if (force) render(true);
  delay(20);
}
