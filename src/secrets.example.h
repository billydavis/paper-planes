#pragma once
// Copy this file to src/secrets.h (which git ignores) and fill it in.
// With WIFI_SSID left empty the board runs in demo mode: it cycles through
// built-in sample flights without touching the network.

#define WIFI_SSID ""
#define WIFI_PASS ""

// The flight-tracker server, started with FT_HOST=0.0.0.0 (see docs/epaper-api.md).
#define SERVER_URL "http://192.168.4.142:8000"

// Home location and search radius (nautical miles).
#define HOME_LAT 38.9531
#define HOME_LON -77.4565
#define RADIUS_NM 15

// POSIX time zone for the clock on the idle screen. US Eastern shown;
// Central "CST6CDT,M3.2.0,M11.1.0", Mountain "MST7MDT,M3.2.0,M11.1.0",
// Pacific "PST8PDT,M3.2.0,M11.1.0".
#define TZ_INFO "EST5EDT,M3.2.0,M11.1.0"
