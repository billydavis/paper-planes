#include "app/mutes.h"

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include "app/constants.h"
#include "app/log.h"

static const char *NS = "mutes";
static MuteList list;
static bool changed = false;
static SemaphoreHandle_t lock;

struct Locked {
  Locked() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Locked() { xSemaphoreGive(lock); }
};

uint32_t mutesNow() {
  time_t t = time(nullptr);
  return t > (time_t)CLOCK_VALID ? (uint32_t)t : 0;
}

// Under `lock`. Mutes change rarely, so the whole list is rewritten each time.
static void save() {
  MuteList::Entry raw[MuteList::MAX];
  for (int i = 0; i < MuteList::MAX; i++) raw[i] = list.at(i);
  Preferences p;
  p.begin(NS, false);
  p.putBytes("list", raw, sizeof(raw));
  p.end();
  changed = true;
}

void mutesBegin() {
  lock = xSemaphoreCreateMutex();
  MuteList::Entry raw[MuteList::MAX];
  Preferences p;
  p.begin(NS, false);
  bool ok = p.isKey("list") && p.getBytesLength("list") == sizeof(raw) && p.getBytes("list", raw, sizeof(raw)) == sizeof(raw);
  p.end();
  if (!ok) return;
  for (MuteList::Entry &e : raw) {
    e.hex[sizeof(e.hex) - 1] = 0;
    e.label[sizeof(e.label) - 1] = 0;
    if (e.hex[0]) list.add(e.hex, e.until, 0, e.label);
  }
  if (list.count()) LOGI("mutes", "%d aircraft muted", list.count());
}

MuteResult muteAdd(const char *hex, int hours, const char *label) {
  uint32_t now = mutesNow();
  if (hours > 0 && !now) return MuteResult::NoClock;
  uint32_t until = hours > 0 ? now + (uint32_t)hours * 3600 : 0;
  Locked l;
  if (!list.add(hex, until, now, label)) return MuteResult::BadHex;
  save();
  LOGI("mutes", "muted %s %s", hex, hours > 0 ? "for a while" : "until unmuted");
  return MuteResult::Ok;
}

bool muteRemove(const char *hex) {
  Locked l;
  if (!list.remove(hex)) return false;
  save();
  LOGI("mutes", "unmuted %s", hex);
  return true;
}

MuteList mutesSnapshot() {
  uint32_t now = mutesNow();
  Locked l;
  if (now && list.prune(now)) save();
  return list;
}

bool mutesChanged() {
  Locked l;
  bool c = changed;
  changed = false;
  return c;
}
