#pragma once
// Optional. Copy this file to src/secrets.h (which git ignores) to bake in
// starting values. They're only used until settings are saved from the
// board's web page (http://flights.local/settings), which then win.
//
// You can leave all of this alone: with no Wi-Fi set, the board opens a
// "flights-setup" hotspot (password below) and the settings page is at
// http://192.168.4.1.

#define WIFI_SSID ""
#define WIFI_PASS ""

// The board answers at http://<HOSTNAME>.local
#define HOSTNAME "flights"

// Home location and search radius (nautical miles).
#define HOME_LAT 38.9531
#define HOME_LON -77.4565
#define RADIUS_NM 15

// POSIX time zone for the clock on the idle screen. US Eastern shown;
// Central "CST6CDT,M3.2.0,M11.1.0", Mountain "MST7MDT,M3.2.0,M11.1.0",
// Pacific "PST8PDT,M3.2.0,M11.1.0".
#define TZ_INFO "EST5EDT,M3.2.0,M11.1.0"

// Password for the settings and firmware-update pages (user name "admin").
// Empty = anyone on your network can change settings.
#define WEB_PASSWORD ""

// Password of the setup hotspot (8+ characters), shown on the screen. Leave it
// empty and each board makes up its own random one (recommended).
#define SETUP_PASSWORD ""
