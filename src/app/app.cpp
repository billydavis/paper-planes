#include "app/app.h"

#include <WiFi.h>
#include <time.h>

#include "app/constants.h"
#include "app/demo.h"
#include "app/log.h"
#include "app/network.h"
#include "app/settings_store.h"
#include "flights/airport_data.h"
#include "flights/sources.h"
#include "generated/version_gen.h"
#include "ui/display.h"
#include "ui/screen.h"
#include "web/server.h"

static Settings cfg;
static Display display;
static Canvas canvas;

static Flight flight;          // last good result
static bool haveData = false;
static bool feedDown = false;  // the last lookup failed (every ADS-B feed unreachable)
static uint32_t lastGood = 0, lastPoll = 0, lastFull = 0;
static char shownHex[8] = "";  // aircraft on the panel; "" = a message screen
static char lastSeen[48] = ""; // "AA 2417 at 14:32", for the quiet-skies screen

// ---- what the board is doing -----------------------------------------------------
static bool isNight() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;   // never before the clock is set
  return inNight(cfg, t.tm_hour * 60 + t.tm_min);
}

static Mode currentMode() {
  if (inSetupMode()) return Mode::Setup;
  if (isNight()) return Mode::Night;
  if (cfg.demo) return Mode::Demo;
  if (haveData) return millis() - lastGood > STALE_MS ? Mode::Stale : Mode::Live;
  if (WiFi.status() != WL_CONNECTED) return Mode::NoWifi;
  if (feedDown) return Mode::FeedDown;
  return Mode::Starting;
}

// The status line on the web page (its colour comes from the first word).
static void statusText(Mode m, char *out, size_t n) {
  char hm[8];
  switch (m) {
    case Mode::Setup: snprintf(out, n, "setup hotspot"); break;
    case Mode::Night:
      formatTime(cfg.nightEnd, hm, sizeof(hm));
      snprintf(out, n, "night, back on at %s", hm);
      break;
    case Mode::Starting: snprintf(out, n, "starting"); break;
    case Mode::NoWifi: snprintf(out, n, "no Wi-Fi"); break;
    case Mode::FeedDown: snprintf(out, n, "can't reach the flight feeds"); break;
    case Mode::Live: snprintf(out, n, flight.ok ? "live" : "live, quiet skies"); break;
    case Mode::Stale: snprintf(out, n, "stale"); break;
    case Mode::Demo: snprintf(out, n, "demo"); break;
  }
}

static bool clockText(char *out, size_t n, const char *fmt) {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  strftime(out, n, fmt, &t);
  return true;
}

// ---- drawing ------------------------------------------------------------------------
static void render(Mode m, bool force) {
  char host[48], caption[32];
  snprintf(host, sizeof(host), "http://%s.local", cfg.hostname);
  if (m == Mode::Stale) snprintf(caption, sizeof(caption), "NO UPDATE %lu MIN", (millis() - lastGood) / 60000);

  bool wasMessage = !shownHex[0];
  bool full = force || millis() - lastFull > FULL_EVERY_MS;
  bool showFlight = (m == Mode::Live || m == Mode::Stale || m == Mode::Demo) && haveData && flight.ok;

  if (showFlight) {
    if (m != Mode::Stale) snprintf(caption, sizeof(caption), "%s", flight.callsign[0] ? flight.callsign : flight.hex);
    drawFlight(canvas, flight, caption);
    if (wasMessage || strcmp(shownHex, flight.hex)) full = true;   // new aircraft
    snprintf(shownHex, sizeof(shownHex), "%s", flight.hex);
  } else {
    char l1[96], l2[64], foot[40] = "";
    switch (m) {
      case Mode::Setup:
        snprintf(l1, sizeof(l1), "Join Wi-Fi \"%s\", password %s", SETUP_AP_SSID, setupApPassword());
        snprintf(l2, sizeof(l2), "then open http://%s", WiFi.softAPIP().toString().c_str());
        drawMessage(canvas, "SETUP", l1, l2, cfg.wifiSsid[0] ? "COULDN'T JOIN SAVED WI-FI" : "NO WI-FI SAVED YET");
        break;
      case Mode::NoWifi:
        snprintf(l1, sizeof(l1), "Trying to join %s", cfg.wifiSsid);
        drawMessage(canvas, "NO WI-FI", l1, "Restart the board for the setup hotspot", nullptr);
        break;
      case Mode::FeedDown:
        drawMessage(canvas, "NO DATA", "Can't reach the flight feeds", host, nullptr);
        break;
      case Mode::Live:
      case Mode::Stale:
      case Mode::Demo:
        if (haveData) {   // quiet skies
          snprintf(l1, sizeof(l1), "Nothing airborne within %.0f nm", flight.radiusNm);
          if (lastSeen[0]) snprintf(l2, sizeof(l2), "Last seen: %s", lastSeen);
          else snprintf(l2, sizeof(l2), "%s", host);
          char hm[8];
          if (clockText(hm, sizeof(hm), "%H:%M")) snprintf(foot, sizeof(foot), "UPDATED %s", hm);
          if (m == Mode::Stale) snprintf(foot, sizeof(foot), "%s", caption);
          drawMessage(canvas, "QUIET SKIES", l1, l2, foot);
          break;
        }
        // fall through: no data yet
      default:
        drawMessage(canvas, "STARTING", cfg.demo ? "Demo mode" : "Looking for aircraft", host, nullptr);
    }
    if (!wasMessage) full = true;
    shownHex[0] = 0;
  }
  if (display.show(canvas, full) && full) lastFull = millis();
}

static void publish(Mode m) {
  char status[64];
  statusText(m, status, sizeof(status));
  webPublish(flight, haveData && m != Mode::Night, status);
}

// ---- data -----------------------------------------------------------------------------
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
  lastPoll = millis();
  if (cfg.demo) {
    accept(demoNext());
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }
  uint32_t t0 = millis();
  Flight f;
  if (trackNearest(cfg, haveData && flight.ok ? flight.hex : nullptr, f)) {
    feedDown = false;
    accept(f);
    FeedTiming ft = lastFeedTiming();
    LOGI("poll", "%s %s, %lu ms (feed %lu ms%s)", f.ok ? f.flight : "none", f.ok && f.hasRoute ? f.basis : "",
         millis() - t0, (unsigned long)ft.ms, ft.reused ? ", reused connection" : "");
  } else {
    feedDown = true;
    LOGW("poll", "every ADS-B feed failed");
  }
}

static void dumpScreen() {
  uint8_t *buf = (uint8_t *)malloc(Canvas::STRIDE * Canvas::H);
  if (!buf) return;
  display.snapshot(buf);
  Serial.printf("BEGIN CANVAS %d %d\n", Canvas::W, Canvas::H);
  static const char *hex = "0123456789abcdef";
  char line[Canvas::STRIDE * 2 + 1];
  for (int y = 0; y < Canvas::H; y++) {
    for (int i = 0; i < Canvas::STRIDE; i++) {
      uint8_t b = buf[y * Canvas::STRIDE + i];
      line[2 * i] = hex[b >> 4];
      line[2 * i + 1] = hex[b & 15];
    }
    line[Canvas::STRIDE * 2] = 0;
    Serial.println(line);
  }
  Serial.println("END CANVAS");
  free(buf);
}

static void applySettings(const Settings &s) {
  bool moved = s.lat != cfg.lat || s.lon != cfg.lon || s.radiusNm != cfg.radiusNm;
  bool tz = strcmp(s.tz, cfg.tz);
  cfg = s;
  if (tz) networkSetTimeZone(cfg.tz);
  if (moved) {
    haveData = false;
    lastSeen[0] = 0;
  }
  lastPoll = millis() - cfg.pollS * 1000UL;   // look again right away
  LOGI("settings", "updated from the web page");
}

// ---- entry points ---------------------------------------------------------------------
void appBegin() {
  loadSettings(cfg);
  LOGI("boot", "nearest-flight %s, %s", FW_VERSION, cfg.demo ? "demo mode" : cfg.wifiSsid);
  LOGI("boot", "PSRAM %u KB, free heap %u KB", ESP.getPsramSize() / 1024, ESP.getFreeHeap() / 1024);
  display.begin();
  airportDataBegin();
  networkBegin(cfg);
  webBegin(cfg, inSetupMode(), display);
  Mode m = currentMode();
  if (m != Mode::Setup && m != Mode::Night) poll();
  m = currentMode();
  if (m != Mode::Night) render(m, true);   // at night, appTick() puts the panel to sleep
  publish(m);
}

void appTick() {
  bool force = false;
  while (Serial.available()) {
    int ch = Serial.read();
    if (ch == 'd') dumpScreen();
    if (ch == 'f') force = true;
    // In live mode the sample stays up until the next poll replaces it.
    if (ch == 'n') {
      accept(demoNext());
      if (cfg.demo) lastPoll = millis();
      force = true;
    }
  }
  Settings changed;
  if (webSettingsChanged(changed)) {
    applySettings(changed);
    force = true;
  }

  Mode m = currentMode();
  switch (m) {
    case Mode::Setup:
      networkTickSetup(cfg);
      if (force) { render(m, true); publish(m); }
      break;

    case Mode::Night:
      if (!display.asleep()) {
        LOGI("night", "screen off");
        display.sleep();
        haveData = false;   // what we had will be hours old by morning
        lastSeen[0] = 0;
        shownHex[0] = 0;
        publish(m);
      }
      delay(200);
      return;

    default:
      if (display.asleep()) {   // morning
        LOGI("night", "screen back on");
        poll();
        m = currentMode();
        render(m, true);
        publish(m);
        return;
      }
      if (m == Mode::Demo && millis() - lastPoll < DEMO_MS) {
        static uint32_t lastNudge = 0;
        if (millis() - lastNudge >= cfg.pollS * 1000UL) {   // exercises partial refresh
          lastNudge = millis();
          demoNudge(flight);
          render(m, force);
          publish(m);
          return;
        }
      } else if (millis() - lastPoll >= (m == Mode::Demo ? DEMO_MS : cfg.pollS * 1000UL)) {
        poll();
        m = currentMode();
        render(m, force);
        publish(m);
        return;
      }
      if (force) { render(m, true); publish(m); }
  }
  delay(20);
}
