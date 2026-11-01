#pragma once
#include "settings.h"

// Settings in flash (NVS). src/secrets.h, if present, supplies the values
// used before anything has been saved from the web page.
void loadSettings(Settings &s);
void saveSettings(const Settings &s);

// The setup hotspot the board opens when it can't join Wi-Fi. The password
// is made up once per board (kept in flash) and shown on the panel, unless
// secrets.h sets SETUP_PASSWORD. Valid after loadSettings().
extern const char *SETUP_AP_SSID;
const char *setupApPassword();
