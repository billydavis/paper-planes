#include "net/https.h"

#include "app/log.h"
#include "generated/certs_gen.h"

// hexdb asks for a User-Agent that says who's calling.
static const char *UA = "paper-planes/2.0 (+https://github.com/billydavis/paper-planes)";

static void hostOf(const char *url, char *out, size_t n) {
  const char *p = strstr(url, "://");
  p = p ? p + 3 : url;
  size_t i = 0;
  for (; p[i] && p[i] != '/' && p[i] != ':' && i < n - 1; i++) out[i] = p[i];
  out[i] = 0;
}

int Https::getJson(const char *url, JsonDocument &doc, const JsonDocument *filter, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  if (!ready) {
    tls.setCACertBundle(CERT_BUNDLE);
    http.setUserAgent(UA);
    http.setReuse(keep);
    http.setConnectTimeout(5000);
    ready = true;
  }
  char h[64];
  hostOf(url, h, sizeof(h));
  if (strcmp(h, host)) {   // never send a request down another host's socket
    tls.stop();
    strcpy(host, h);
  }
  reused = keep && tls.connected();
  int code = request(url, doc, filter, timeoutMs);
  if (code < 0 && reused) {   // the server closed the idle connection: once more, fresh
    LOGD("https", "kept connection to %s went stale, reconnecting", host);
    tls.stop();
    reused = false;
    code = request(url, doc, filter, timeoutMs);
  }
  elapsed = millis() - t0;
  return code;
}

int Https::request(const char *url, JsonDocument &doc, const JsonDocument *filter, uint32_t timeoutMs) {
  doc.clear();
  http.setTimeout(timeoutMs);   // HTTPClient applies it to the socket once connected
  if (!http.begin(tls, url)) return -1;
  http.addHeader("Accept", "application/json");
  int code = http.GET();
  if (code > 0) {
    // Read the whole body (de-chunked by HTTPClient) so nothing is left in
    // the socket to confuse the next request on a kept-open connection.
    String body = http.getString();
    if (code == 200) {
      DeserializationError err = filter
          ? deserializeJson(doc, body, DeserializationOption::Filter(*filter))
          : deserializeJson(doc, body);
      if (err) {
        LOGW("https", "%s: bad JSON (%s)", url, err.c_str());
        doc.clear();
        code = -2;
      }
    } else {
      LOGD("https", "%s: HTTP %d", url, code);
    }
  } else {
    LOGW("https", "%s: %s", url, HTTPClient::errorToString(code).c_str());
  }
  http.end();
  if (code < 0) tls.stop();
  return code;
}
