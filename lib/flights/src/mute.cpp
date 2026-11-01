#include "mute.h"

#include <ctype.h>
#include <string.h>

void normalizeHex(const char *in, char (&out)[8]) {
  int n = 0;
  for (const char *p = in ? in : ""; *p && n < 7; p++)
    if (isxdigit((unsigned char)*p) || (*p == '~' && n == 0)) out[n++] = tolower(*p);
  out[n] = 0;
}

static bool expired(const MuteList::Entry &e, uint32_t now) { return e.until && now && now >= e.until; }

int MuteList::find(const char *hex) const {
  for (int i = 0; i < MAX; i++)
    if (e_[i].hex[0] && !strcmp(e_[i].hex, hex)) return i;
  return -1;
}

bool MuteList::add(const char *hex, uint32_t until, uint32_t now, const char *label) {
  char h[8];
  normalizeHex(hex, h);
  if (!h[0]) return false;
  prune(now);
  int i = find(h);
  if (i < 0)
    for (int j = 0; j < MAX && i < 0; j++)
      if (!e_[j].hex[0]) i = j;
  if (i < 0) {   // full: replace the timed mute ending soonest (or, all permanent, the first)
    i = 0;
    for (int j = 0; j < MAX; j++) {
      uint32_t a = e_[j].until, b = e_[i].until;
      if (a && (!b || a < b)) i = j;
    }
  }
  Entry &e = e_[i];
  if (strcmp(e.hex, h)) e = Entry();   // a new aircraft in this slot
  strcpy(e.hex, h);
  e.until = until;
  if (label && label[0]) {   // re-timing without a label keeps the old one
    int n = 0;
    for (const char *p = label; *p && n < (int)sizeof(e.label) - 1; p++)
      if (isalnum((unsigned char)*p) || *p == '-') e.label[n++] = toupper(*p);
    e.label[n] = 0;
  }
  return true;
}

bool MuteList::remove(const char *hex) {
  char h[8];
  normalizeHex(hex, h);
  int i = h[0] ? find(h) : -1;
  if (i < 0) return false;
  e_[i] = Entry();
  return true;
}

bool MuteList::muted(const char *hex, uint32_t now) const {
  char h[8];
  normalizeHex(hex, h);
  int i = h[0] ? find(h) : -1;
  return i >= 0 && !expired(e_[i], now);
}

bool MuteList::prune(uint32_t now) {
  bool any = false;
  for (Entry &e : e_)
    if (e.hex[0] && expired(e, now)) { e = Entry(); any = true; }
  return any;
}

void MuteList::clear() {
  for (Entry &e : e_) e = Entry();
}

int MuteList::count() const {
  int n = 0;
  for (const Entry &e : e_) n += e.hex[0] != 0;
  return n;
}
