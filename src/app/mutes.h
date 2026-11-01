#pragma once
#include "mute.h"

// The board's muted aircraft (lib/flights/src/mute.h), kept in flash so a
// restart or firmware update doesn't bring them back. Safe to call from the
// web task and the main task.

void mutesBegin();

enum class MuteResult { Ok, BadHex, NoClock };
// `hours` = 0 mutes until unmuted. Timed mutes need the clock (NTP) set.
MuteResult muteAdd(const char *hex, int hours, const char *label);
bool muteRemove(const char *hex);

// The current list, expired mutes dropped.
MuteList mutesSnapshot();
// Unix time now, or 0 while the clock isn't set.
uint32_t mutesNow();

// True once after the list changed (the main loop then looks again).
bool mutesChanged();
