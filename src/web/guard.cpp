#include "web/guard.h"

#include <WiFi.h>

const char *WEB_USER = "admin";

static WebServer *srv = nullptr;
static bool setup_ = false;
static char hostname[40] = "";   // "<name>.local"
static char webPass[40] = "";

void guardBegin(WebServer &server, bool setupMode) {
  srv = &server;
  setup_ = setupMode;
  const char *headers[] = {"Host", "Origin", "Referer"};
  server.collectHeaders(headers, 3);
}

void guardConfigure(const char *host, const char *pass) {
  snprintf(hostname, sizeof(hostname), "%s.local", host);
  snprintf(webPass, sizeof(webPass), "%s", pass);
}

bool hostOk(const String &host) {
  String h = host;
  int colon = h.indexOf(':');
  if (colon >= 0) h = h.substring(0, colon);
  h.toLowerCase();
  return h == hostname || h == WiFi.localIP().toString() || h == WiFi.softAPIP().toString();
}

std::function<void()> guarded(std::function<void()> fn) {
  return [fn] {
    if (hostOk(srv->header("Host"))) { fn(); return; }
    if (setup_) {
      srv->sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/settings");
      srv->send(302, "text/plain", "");
    } else {
      srv->send(403, "text/plain", "unknown host");
    }
  };
}

bool sameOrigin() {
  String o = srv->header("Origin");
  if (!o.length() || o == "null") o = srv->header("Referer");
  if (!o.startsWith("http://")) return false;
  o = o.substring(7);
  int slash = o.indexOf('/');
  if (slash >= 0) o = o.substring(0, slash);
  return hostOk(o);
}

bool passwordSet() { return webPass[0] != 0; }

bool authorized() {
  if (!webPass[0] || srv->authenticate(WEB_USER, webPass)) return true;
  srv->requestAuthentication(BASIC_AUTH, "flights", "Settings password");
  return false;
}

bool authorizedPost() {
  if (!sameOrigin()) {
    srv->send(403, "application/json", "{\"error\":\"cross-site request refused\"}");
    return false;
  }
  return authorized();
}

bool authorizedQuiet() {
  return hostOk(srv->header("Host")) && sameOrigin() && (!webPass[0] || srv->authenticate(WEB_USER, webPass));
}
