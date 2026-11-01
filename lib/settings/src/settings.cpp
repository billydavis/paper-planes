#include "settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR(key, member, flags) \
  {key, FieldType::Str, offsetof(Settings, member), sizeof(Settings::member), 0, 0, flags}
#define SECRET(key, member, flags) \
  {key, FieldType::Secret, offsetof(Settings, member), sizeof(Settings::member), 0, 0, flags}
#define NUM(key, type, member, lo, hi) \
  {key, FieldType::type, offsetof(Settings, member), sizeof(Settings::member), lo, hi, 0}

// The NVS keys are the ones earlier firmware used, so saved settings carry over.
const Field SETTINGS_FIELDS[] = {
    STR("ssid", wifiSsid, F_RESTART),
    SECRET("pass", wifiPass, F_RESTART),
    STR("host", hostname, F_RESTART | F_HOSTNAME),
    NUM("lat", Double, lat, -90, 90),
    NUM("lon", Double, lon, -180, 180),
    NUM("radius", Int, radiusNm, 1, 100),
    NUM("poll", Int, pollS, 5, 300),
    STR("tz", tz, F_KEEP_EMPTY),
    SECRET("webpass", webPass, 0),
    {"demo", FieldType::Bool, offsetof(Settings, demo), sizeof(bool), 0, 0, F_RESTART},
    {"night", FieldType::Bool, offsetof(Settings, nightOn), sizeof(bool), 0, 0, 0},
    NUM("nstart", Time, nightStart, 0, 1439),
    NUM("nend", Time, nightEnd, 0, 1439),
};
const int SETTINGS_FIELD_COUNT = sizeof(SETTINGS_FIELDS) / sizeof(SETTINGS_FIELDS[0]);

int parseTime(const char *v) {
  int h, m;
  char extra;
  if (!v || sscanf(v, "%d:%d%c", &h, &m, &extra) != 2) return -1;
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

void formatTime(int minutes, char *out, size_t n) {
  minutes = ((minutes % 1440) + 1440) % 1440;
  snprintf(out, n, "%02d:%02d", minutes / 60, minutes % 60);
}

static void copyTrimmed(char *dst, size_t n, const char *v) {
  while (isspace((unsigned char)*v)) v++;
  size_t len = strlen(v);
  while (len && isspace((unsigned char)v[len - 1])) len--;
  if (len > n - 1) len = n - 1;
  memcpy(dst, v, len);
  dst[len] = 0;
}

static bool equal(const Settings &a, const Settings &b, const Field &f) {
  return !memcmp((const char *)&a + f.offset, (const char *)&b + f.offset, f.size);
}

bool applyForm(Settings &s, FormGetter get, void *ctx) {
  const Settings old = s;
  char clearKey[32];
  for (int i = 0; i < SETTINGS_FIELD_COUNT; i++) {
    const Field &f = SETTINGS_FIELDS[i];
    const char *v = get(f.key, ctx);
    switch (f.type) {
      case FieldType::Str: {
        if (!v) break;
        char tmp[96];
        copyTrimmed(tmp, sizeof(tmp), v);
        if (f.flags & F_HOSTNAME) {
          for (char *p = tmp; *p; p++) {
            *p = tolower((unsigned char)*p);
            if (!isalnum((unsigned char)*p) && *p != '-') *p = '-';
          }
          if (!tmp[0]) strcpy(tmp, "flights");
        }
        if (!tmp[0] && (f.flags & F_KEEP_EMPTY)) break;
        snprintf(fieldStr(s, f), f.size, "%s", tmp);
        break;
      }
      case FieldType::Secret:
        snprintf(clearKey, sizeof(clearKey), "%s_clear", f.key);
        if (get(clearKey, ctx)) fieldStr(s, f)[0] = 0;
        else if (v && v[0]) snprintf(fieldStr(s, f), f.size, "%s", v);   // passwords keep spaces
        break;
      case FieldType::Int:
        if (v && *v) {
          char *end;
          long n = strtol(v, &end, 10);
          if (end != v) fieldRef<int>(s, f) = (int)(n < f.min ? f.min : n > f.max ? f.max : n);
        }
        break;
      case FieldType::Double:
        if (v && *v) {
          char *end;
          double d = strtod(v, &end);
          if (end != v && d == d) fieldRef<double>(s, f) = d < f.min ? f.min : d > f.max ? f.max : d;
        }
        break;
      case FieldType::Bool:
        fieldRef<bool>(s, f) = v != nullptr;
        break;
      case FieldType::Time: {
        int m = parseTime(v);
        if (m >= 0) fieldRef<int>(s, f) = m;
        break;
      }
    }
  }
  // A different network with no password given is an open network, not the
  // old network's password.
  const char *pass = get("pass", ctx);
  if (strcmp(s.wifiSsid, old.wifiSsid) && !(pass && pass[0])) s.wifiPass[0] = 0;

  bool restart = false;
  for (int i = 0; i < SETTINGS_FIELD_COUNT; i++)
    if ((SETTINGS_FIELDS[i].flags & F_RESTART) && !equal(s, old, SETTINGS_FIELDS[i])) restart = true;
  return restart;
}

void settingsToJson(const Settings &s, JsonObject out) {
  char key[32], hm[8];
  for (int i = 0; i < SETTINGS_FIELD_COUNT; i++) {
    const Field &f = SETTINGS_FIELDS[i];
    switch (f.type) {
      case FieldType::Str: out[f.key] = fieldStr(s, f); break;
      case FieldType::Secret:
        snprintf(key, sizeof(key), "%s_set", f.key);
        out[key] = fieldStr(s, f)[0] != 0;
        break;
      case FieldType::Int: out[f.key] = fieldRef<int>(s, f); break;
      case FieldType::Double: out[f.key] = fieldRef<double>(s, f); break;
      case FieldType::Bool: out[f.key] = fieldRef<bool>(s, f); break;
      case FieldType::Time:
        formatTime(fieldRef<int>(s, f), hm, sizeof(hm));
        out[f.key] = hm;
        break;
    }
  }
}

bool inNight(const Settings &s, int m) {
  if (!s.nightOn || s.nightStart == s.nightEnd) return false;
  if (s.nightStart < s.nightEnd) return m >= s.nightStart && m < s.nightEnd;   // e.g. 01:00-05:00
  return m >= s.nightStart || m < s.nightEnd;                                  // e.g. 22:00-07:00
}
