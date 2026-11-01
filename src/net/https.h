#pragma once
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "app/constants.h"

// HTTPS GET of a JSON document, certificates checked against the roots in
// generated/certs_gen.h.
//
// With keepAlive, the TLS connection stays open between requests to the
// same host, so polling the ADS-B feed every few seconds doesn't pay for a
// new handshake (~3 s on this chip) each time. A request to another host
// closes it first: HTTPClient would otherwise reuse the open socket for the
// wrong host.
class Https {
 public:
  explicit Https(bool keepAlive) : keep(keepAlive) {}

  // `filter` keeps only the fields the caller needs. Returns the HTTP status,
  // or a negative number for a network/TLS failure; `doc` holds the parsed
  // body only when the result is 200 (else it's null).
  int getJson(const char *url, JsonDocument &doc, const JsonDocument *filter = nullptr,
              uint32_t timeoutMs = HTTP_TIMEOUT_MS);

  uint32_t lastMs() const { return elapsed; }   // duration of the last request
  bool lastReused() const { return reused; }    // it went over a kept-open connection

 private:
  int request(const char *url, JsonDocument &doc, const JsonDocument *filter, uint32_t timeoutMs);
  WiFiClientSecure tls;
  HTTPClient http;
  bool keep;
  bool ready = false;
  char host[64] = "";
  uint32_t elapsed = 0;
  bool reused = false;
};
