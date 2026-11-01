#pragma once
#include "settings.h"

// Wi-Fi: join the saved network, or open the setup hotspot when there is
// none or it can't be joined. Also starts NTP with the saved time zone.

// Returns true if joined; false = running the setup hotspot.
bool networkBegin(const Settings &s);
bool inSetupMode();
// In setup mode: keep retrying the saved network; restarts the board once
// it's back. Call from the loop.
void networkTickSetup(const Settings &s);
// New time zone from the settings page.
void networkSetTimeZone(const char *tz);
// The clock has been set (by NTP).
bool clockValid();
