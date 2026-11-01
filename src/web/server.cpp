#include "web/server.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

#include "app/log.h"
#include "app/mutes.h"
#include "app/settings_store.h"
#include "flights/airport_data.h"
#include "generated/version_gen.h"
#include "generated/web_assets_gen.h"
#include "web/bmp.h"
#include "web/guard.h"

static WebServer server(80);
static DNSServer dns;
static SemaphoreHandle_t lock;
static bool setup_ = false;
static const Display *panel = nullptr;

// Shared with the main task, under `lock`.
static Settings settings;
static bool settingsDirty = false;
static Flight flight;
static bool haveData = false;
static char status[64] = "starting";

struct Locked {
  Locked() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Locked() { xSemaphoreGive(lock); }
};

static void mutesToJson(JsonDocument &doc);

static void sendJson(int code, JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

// ---- pages ------------------------------------------------------------------------
static const WebAsset *asset(const char *path) {
  for (int i = 0; i < WEB_ASSET_COUNT; i++)
    if (!strcmp(WEB_ASSETS[i].path, path)) return &WEB_ASSETS[i];
  return nullptr;
}

static void sendAsset(const char *path) {
  const WebAsset *a = asset(path);
  if (!a) { server.send(404, "text/plain", "not found"); return; }
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", strstr(a->mime, "css") ? "max-age=3600" : "no-cache");
  server.send_P(200, a->mime, (const char *)a->gz, a->len);
}

static void handleScreen() {
  uint8_t *buf = (uint8_t *)malloc(Canvas::STRIDE * Canvas::H);
  if (!buf) { server.send(503, "text/plain", "out of memory"); return; }
  panel->snapshot(buf);
  sendBmp(server, buf, Canvas::W, Canvas::H);
  free(buf);
}

// ---- JSON -------------------------------------------------------------------------
static void handleNearest() {
  JsonDocument doc;
  {
    Locked l;
    if (!haveData) { server.send(503, "application/json", "{\"status\":\"starting\"}"); return; }
    flightToJson(flight, doc);
  }
  // No CORS header on purpose: other websites you visit must not read this
  // (aircraft position + bearing + distance give away where home is).
  sendJson(200, doc);
}

static void handleState() {
  JsonDocument doc, fj;
  Settings s;
  {
    Locked l;
    if (haveData) flightToJson(flight, fj);
    s = settings;
    doc["status"] = status;
  }
  doc["frame"] = panel->frameNo();
  if (haveData) doc["flight"] = fj; else doc["flight"] = nullptr;
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["hostname"] = String(s.hostname) + ".local";
  dev["ip"] = setup_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  dev["mode"] = setup_ ? "setup" : "station";
  dev["ssid"] = s.wifiSsid;
  dev["rssi"] = WiFi.RSSI();
  dev["uptime_s"] = millis() / 1000;
  dev["heap"] = ESP.getFreeHeap();
  dev["psram"] = ESP.getFreePsram();
  dev["version"] = FW_VERSION;
  dev["airports"] = airportDataStatus();
  dev["home"]["lat"] = s.lat;
  dev["home"]["lon"] = s.lon;
  dev["radius_nm"] = s.radiusNm;
  dev["demo"] = s.demo;
  if (s.nightOn) {
    char a[8], b[8];
    formatTime(s.nightStart, a, sizeof(a));
    formatTime(s.nightEnd, b, sizeof(b));
    dev["night"] = String(a) + "-" + b;
  }
  mutesToJson(doc);
  sendJson(200, doc);
}

// ---- mutes: aircraft kept off the screen (app/mutes.h) --------------------------------
static void mutesToJson(JsonDocument &doc) {
  MuteList list = mutesSnapshot();
  JsonArray arr = doc["mutes"].to<JsonArray>();
  for (int i = 0; i < MuteList::MAX; i++) {
    const MuteList::Entry &e = list.at(i);
    if (!e.hex[0]) continue;
    JsonObject o = arr.add<JsonObject>();
    o["hex"] = e.hex;
    o["label"] = e.label;
    if (e.until) o["until"] = e.until; else o["until"] = nullptr;   // null = until unmuted
  }
  doc["clock_set"] = mutesNow() != 0;
}

static void handleMutes() {
  JsonDocument doc;
  mutesToJson(doc);
  sendJson(200, doc);
}

// POST hex=a1b2c3&hours=3[&label=N123AB]. hours=0 (or missing) = until unmuted.
static void handleMute() {
  if (!authorizedPost()) return;
  int hours = server.hasArg("hours") ? server.arg("hours").toInt() : 0;
  if (hours < 0 || hours > 24 * 365) { server.send(400, "text/plain", "hours must be 0 to 8760"); return; }
  switch (muteAdd(server.arg("hex").c_str(), hours, server.arg("label").c_str())) {
    case MuteResult::BadHex: server.send(400, "text/plain", "hex missing or invalid"); return;
    case MuteResult::NoClock: server.send(503, "text/plain", "clock not set yet; try again in a minute"); return;
    case MuteResult::Ok: break;
  }
  handleMutes();
}

// POST hex=a1b2c3
static void handleUnmute() {
  if (!authorizedPost()) return;
  if (!muteRemove(server.arg("hex").c_str())) { server.send(404, "text/plain", "not muted"); return; }
  handleMutes();
}

// ---- settings -----------------------------------------------------------------------
static void handleSettingsGet() {
  if (!authorized()) return;
  JsonDocument doc;
  {
    Locked l;
    settingsToJson(settings, doc.to<JsonObject>());
  }
  doc["setup_mode"] = setup_;
  sendJson(200, doc);
}

static void handleNetworks() {
  if (!authorized()) return;
  JsonDocument doc;
  JsonArray list = doc.to<JsonArray>();
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) list.add(WiFi.SSID(i));
  WiFi.scanDelete();
  sendJson(200, doc);
}

static const char *formArg(const char *key, void *) {
  static String v;   // the web server is single-threaded; one value at a time is enough
  if (!server.hasArg(key)) return nullptr;
  v = server.arg(key);
  return v.c_str();
}

static void handleSettingsSave() {
  if (!authorizedPost()) return;
  Settings s;
  { Locked l; s = settings; }
  bool restart = applyForm(s, formArg, nullptr) || setup_;
  saveSettings(s);
  {
    Locked l;
    settings = s;
    settingsDirty = true;
  }
  guardConfigure(s.hostname, s.webPass);
  JsonDocument doc;
  doc["ok"] = true;
  doc["restart"] = restart;
  doc["url"] = String("http://") + s.hostname + ".local/";
  sendJson(200, doc);
  if (restart) {
    delay(800);
    ESP.restart();
  }
}

static void handleRestart() {
  if (!authorizedPost()) return;
  server.send(200, "text/plain", "restarting");
  delay(500);
  ESP.restart();
}

// ---- /update: firmware or airport data over Wi-Fi --------------------------------------
// Whoever can flash firmware owns the board, so this needs a settings
// password to be set (not just "none set = open"), plus the same-origin check.
static bool updateAllowed() {
  if (!passwordSet()) {
    server.send(403, "text/plain", "Set a settings password first (Settings page); updates need one.");
    return false;
  }
  return authorized();
}

static bool uploadOk = false;   // the current upload passed the checks

static void handleUpdatePage() {
  if (updateAllowed()) sendAsset("/update");
}

static void handleUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    // Checked here, before a single byte is written to flash. (No response
    // yet: handleUploadDone says why once the upload has been read.)
    bool data = server.arg("target") == "airports";
    uploadOk = passwordSet() && authorizedQuiet();
    if (uploadOk && data) airportDataRelease();   // stop reading it before it's rewritten
    if (uploadOk && !Update.begin(UPDATE_SIZE_UNKNOWN, data ? U_SPIFFS : U_FLASH)) uploadOk = false;
    if (uploadOk) LOGI("update", "receiving %s (%s)", up.filename.c_str(), data ? "airport data" : "firmware");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadOk && Update.write(up.buf, up.currentSize) != up.currentSize) uploadOk = false;
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadOk && !Update.end(true)) uploadOk = false;
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (Update.isRunning()) Update.abort();
    uploadOk = false;
  }
}

static void handleUploadDone() {
  if (!uploadOk) {
    if (Update.isRunning()) Update.abort();
    if (!sameOrigin()) server.send(403, "text/plain", "cross-site request refused");
    else if (updateAllowed())   // (sends its own refusal when it fails)
      server.send(400, "text/plain", Update.hasError() ? Update.errorString() : "update failed");
    return;
  }
  server.send(200, "text/plain", "Update installed. Restarting...");
  delay(800);
  ESP.restart();
}

// ---- setup -------------------------------------------------------------------------------
static void serverTask(void *) {
  for (;;) {
    if (setup_) dns.processNextRequest();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void webBegin(const Settings &s, bool setupMode, const Display &display) {
  lock = xSemaphoreCreateMutex();
  settings = s;
  setup_ = setupMode;
  panel = &display;
  guardBegin(server, setupMode);
  guardConfigure(s.hostname, s.webPass);

  server.on("/", HTTP_GET, guarded([] { sendAsset("/"); }));
  server.on("/style.css", HTTP_GET, guarded([] { sendAsset("/style.css"); }));
  server.on("/settings", HTTP_GET, guarded([] { if (authorized()) sendAsset("/settings"); }));
  server.on("/settings", HTTP_POST, guarded(handleSettingsSave));
  server.on("/update", HTTP_GET, guarded(handleUpdatePage));
  server.on("/update", HTTP_POST, guarded(handleUploadDone), handleUpload);
  server.on("/restart", HTTP_POST, guarded(handleRestart));
  server.on("/screen.bmp", HTTP_GET, guarded(handleScreen));
  server.on("/api/state", HTTP_GET, guarded(handleState));
  server.on("/api/nearest", HTTP_GET, guarded(handleNearest));
  server.on("/api/settings", HTTP_GET, guarded(handleSettingsGet));
  server.on("/api/networks", HTTP_GET, guarded(handleNetworks));
  server.on("/api/mutes", HTTP_GET, guarded(handleMutes));
  server.on("/api/mute", HTTP_POST, guarded(handleMute));
  server.on("/api/unmute", HTTP_POST, guarded(handleUnmute));
  server.onNotFound([] {
    if (setup_) {   // captive portal: phones pop the settings page up on joining
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/settings");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "not found");
    }
  });

  if (setupMode) dns.start(53, "*", WiFi.softAPIP());
  server.begin();
  if (MDNS.begin(s.hostname)) {
    MDNS.addService("http", "tcp", 80);
    LOGI("web", "http://%s.local/", s.hostname);
  }
  xTaskCreatePinnedToCore(serverTask, "web", 8192, nullptr, 1, nullptr, 0);
}

void webPublish(const Flight &f, bool have, const char *st) {
  Locked l;
  flight = f;
  haveData = have;
  snprintf(status, sizeof(status), "%s", st);
}

bool webSettingsChanged(Settings &out) {
  Locked l;
  if (!settingsDirty) return false;
  settingsDirty = false;
  out = settings;
  return true;
}
