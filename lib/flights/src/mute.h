#pragma once
#include <stdint.h>

// Aircraft hidden from the screen for a while (or for good): a flight school's
// trainer circling the field next door, say. Keyed by ICAO hex, the one ID an
// airframe always sends (training flights often have no callsign).
//
// Times are Unix seconds. `until` = 0 means muted until unmuted. No I/O here;
// src/app/mutes.cpp keeps the list in flash and shares it between tasks.

// `in` reduced to what a hex can hold (lowercase hex digits, a leading "~"
// for non-ICAO addresses, 7 chars max). Same rules as the feed's hex.
void normalizeHex(const char *in, char (&out)[8]);

class MuteList {
 public:
  static const int MAX = 16;
  struct Entry {
    char hex[8] = "";     // "" = empty slot
    char label[12] = "";  // registration or flight number, for people to read
    uint32_t until = 0;
  };

  // Adds or re-times `hex`. When the list is full, the mute ending soonest
  // makes room. False for a hex that normalizes to nothing. `label` keeps
  // letters, digits and "-" only.
  bool add(const char *hex, uint32_t until, uint32_t now, const char *label = nullptr);
  bool remove(const char *hex);
  // `now` = 0: the clock isn't set yet; timed mutes then count as running.
  bool muted(const char *hex, uint32_t now) const;
  // Drops expired mutes; true if any were dropped.
  bool prune(uint32_t now);
  void clear();

  int count() const;
  const Entry &at(int i) const { return e_[i]; }   // 0..MAX-1, may be empty

 private:
  Entry e_[MAX];
  int find(const char *hex) const;
};
