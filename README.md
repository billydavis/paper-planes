# paper-planes

The aircraft nearest to home, on a CrowPanel 5.79" e-paper display (ESP32-S3).
Stand-alone: the board looks the flights up itself and serves the same screen
and its settings at http://flights.local.

Hardware: [Elecrow CrowPanel ESP32 E-Paper 5.79-inch HMI Display](https://elecrow.com/wiki/CrowPanel_ESP32_E-paper_5.79-inch_HMI_Display.html)

## Board facts (verified against the physical unit on COM5)

- MCU: ESP32-S3-WROOM-1, 8MB flash (quad), 8MB embedded PSRAM, CH340 USB-serial
- Panel: 272x792 B&W, dual SSD1683 driver chips (split 396x272 + 396x272), SPI
- Built-in controls: MENU button, EXIT button, 3-way rotary switch (up/down/confirm)
- TF/SD card slot on its own SPI bus (MOSI 40, MISO 13, CLK 39, CS 10)
- E-paper control pins (bit-banged, from Elecrow's own reference code):
  `SCK=12, MOSI=11, RES=47, DC=46, CS=45, BUSY=48`
  GPIO7 must be driven HIGH to enable the panel's power rail before it responds.

## Measured refresh timings (this physical unit, `src/main.cpp`)

| Mode | Function | Measured |
|---|---|---|
| Full refresh | `EPD_Update()` | ~2120 ms |
| Fast refresh | `EPD_FastUpdate()` | ~1995 ms |
| Partial refresh | `EPD_PartUpdate()` | ~642 ms |

Timed end-to-end including the BUSY-pin wait, so these reflect real panel completion, not just SPI transfer time.

## Flash backup

A full 8MB flash backup of the board's original (as-purchased) firmware is saved at
`%USERPROFILE%\Documents\esp32_backups\crowpanel_579_full_8MB_backup_2026-10-26.bin`.

Restore with:
```
python -m esptool --port COM5 write-flash 0x0 crowpanel_579_full_8MB_backup_2026-10-26.bin
```

## The display (current firmware)

Shows the closest airborne aircraft. The board does all the work itself, with
no PC in the loop: it asks the free ADS-B feeds (adsb.lol, then adsb.fi) for
aircraft near home, looks up the airframe (hexdb, adsbdb) and route, and draws
it. The decision rules (`lib/flights/src/route.cpp`) are a port of
flight-tracker's `/api/epaper/nearest` (`src/server/epaper.py` + `sources.py`):
same sources, same route-trust rules (`confirmed` / `scheduled` / `track` /
`mismatch`), same departure memory, checked against flight-tracker's own
answers by the parity tests. Plan and mockups: `docs/flight-display-plan.md`,
`docs/mockups/`.

The ADS-B feed connection is kept open between polls, so a routine poll
takes ~0.4 s instead of ~3.4 s (a new TLS handshake). A new aircraft costs a
few more seconds for its hexdb/adsbdb lookups, which are cached.

**Night hours.** On by default, 22:00 to 07:00 (settings page). The panel is
wiped to white, put into deep sleep and its power cut; no lookups run. The web
page keeps working. At the end of the window it looks up the nearest plane
and redraws from a clean full refresh. Needs the clock (NTP), so it never
sleeps before the time is known.

**Web page.** The board serves `http://flights.local` (mDNS; the name is a
setting):
- `/` the frame on the panel right now, plus the flight details and device status
- `/settings` Wi-Fi, home location, radius, poll interval, time zone, night hours,
  settings password, demo mode. Saved to flash.
- `/update` upload a new `firmware.bin`, or new airport data (`airports.bin`),
  over Wi-Fi (only once a settings password is set)
- `/api/nearest` the current flight as JSON, same shape as flight-tracker's
  endpoint (plus `lat`, `lon`); `/api/state` adds device status;
  `/api/settings` the settings (no secrets)
- `/screen.bmp` the panel's frame as an image

The pages are ordinary files in `web/`; the build gzips them into the firmware
(`tools/webgen.py`).

If it has no Wi-Fi saved, or can't join the saved network within 20 s, it
opens a **setup hotspot** `flights-setup`. Its password is made up by each
board and shown on the panel (and on the serial log). Join it and the
settings page pops up (or open http://192.168.4.1).

Security: set a settings password (user `admin`) on the settings page;
without one anyone on your network can change settings, and updates stay
off. The server only answers to its own name/IP (blocks DNS rebinding),
refuses form posts from other sites (CSRF), and sends no CORS header, so other
web pages can't read it. Plain HTTP on the LAN: the password can be sniffed by
someone already on your network. Settings, including the Wi-Fi password, are
stored unencrypted in the board's flash, and `secrets.h` values are compiled
into `firmware.bin`, so don't share a build made with a real `secrets.h`.

### Building and flashing

1. Optional: copy `src/secrets.example.h` to `src/secrets.h` to bake in
   starting values (Wi-Fi, location, ...). Settings saved from the web page win.
2. `pio run -t upload` builds, then flashes the firmware **and** the airport
   data partition over USB (`SKIP_AIRPORTS=1` to flash just the firmware;
   `pio run -t uploadairports` for just the data).
3. Later updates can go over Wi-Fi at http://flights.local/update:
   `.pio/build/esp32-s3-devkitc-1/firmware.bin` (~1.5 MB) and, only when the
   airport list changes, `src/generated/airports.bin`.

The build (`tools/pio_generate.py`) regenerates `src/generated/` when its
inputs change:
- fonts (`tools/fontgen.py`; Pillow and the Windows fonts)
- airline logos (`tools/logogen.py`, see below)
- airport data, airline names and root certificates (`tools/datagen.py`;
  `pip install certifi cryptography`). These come from flight-tracker
  (`../flight-tracker`, or `FLIGHT_TRACKER_DIR`): `data/airports.csv`,
  `airlines.js`, the IATA map in `epaper.py`. Without a flight-tracker checkout
  it uses `tools/airlines_snapshot.json` and downloads `airports.csv` from
  OurAirports, so a fresh clone still builds.
- the web pages (`tools/webgen.py`)
- `version_gen.h` (`git describe`; shown on the web page and in the boot log)

Flash layout (`partitions.csv`): two 2.6 MB firmware slots, a 2.5 MB
`airports` partition (the ~1.5 MB airport image, checked by CRC at boot), NVS
for settings. The chip's 8 MB PSRAM is enabled (`dio_opi`), which leaves
~224 KB of internal heap free.

### Tests

```
pio test -e native
```

Runs on the PC (needs a host `g++`; on Windows, WinLibs:
`winget install BrechtSanders.WinLibs.POSIX.UCRT`), no board needed:
- `test_parity`: every fixture in `test/fixtures/` holds the upstream replies
  flight-tracker's `nearest()` got and the answer it gave; the C++ rules must
  reach the same answer. `python tools/make_fixtures.py` regenerates the
  synthetic ones (one per route rule); `--live` also records real traffic
  around a few big airports. Rerun it when flight-tracker's rules change.
- `test_flights`: geometry, accent folding, the airport image (lookups, CRC,
  bounds), the flight JSON, departure memory, URL-safe hex/callsigns
- `test_settings`: the settings table and form rules, night window
- `test_panel`: the fast canvas-to-panel conversion against the vendor
  driver's per-pixel formula

Logos: `tools/logogen.py` downloads each airline's logo once from
[Jxck-S/airline-logos](https://github.com/Jxck-S/airline-logos) (by ICAO code,
for every airline in flight-tracker's `airlines.js` plus a few bizjet
operators) into `logos/raw/`, dithers it to 1-bit and writes
`src/generated/logos_gen.h`. `logos/preview.png` shows them all as the panel
will. Drop `logos/custom/<ICAO>.png` in to replace one, then rerun the script.
`logos/` is git-ignored: the logos are the airlines' trademarks. Aircraft with
no logo get a silhouette by aircraft class.

Refresh: when the aircraft changes (~3.3 s) the panel is wiped to white and
the new screen drawn from that clean state, which is Elecrow's own sequence. A
plain full refresh of an image left the previous screen ghosted. Partial
refresh (~0.9 s) when only the numbers change, and a forced clean redraw every
20 partials or 30 minutes.

Serial commands (115200): `d` dump screen, `f` force full refresh, `n` next demo flight
(in live mode it shows until the next poll, handy for checking a logo).
`python tools/grab_screen.py NAME [--next] [--compare mockup_1bit.png]` saves the
panel's current frame to `docs/screens/` (and diffs it against a mockup).
Log detail: build flag `-DLOG_LEVEL=3` for debug output (default 2).

## Layout

```
src/
  main.cpp              setup()/loop(): hands over to app/
  app/                  what the board is doing
    app.cpp             the Mode (setup, night, live, stale, ...), polling, drawing
    network.cpp         Wi-Fi, setup hotspot, NTP
    settings_store.cpp  settings in flash (NVS), setup-hotspot password
    demo.cpp            built-in sample flights
    constants.h         timings and cache lifetimes
    log.h               LOGI/LOGW/... with a level
  flights/
    sources.cpp         live lookups: feed, hexdb, adsbdb (HTTP + caches)
    airport_data.cpp    maps the airport partition, over-the-air data updates
  net/https.cpp         HTTPS GET + JSON, certificates checked, keep-alive
  ui/                   canvas, screen layouts, fonts/logos, the e-paper panel
  web/
    server.cpp          routes and handlers
    guard.cpp           Host / Origin / password checks
    bmp.cpp             the panel frame as a BMP
  secrets.h             optional first-boot settings (git-ignored)
  generated/            build output (git-ignored)
lib/
  flights/              route rules, airport image, flight record (no I/O)
  settings/             the settings table: one row per setting
  panel/                canvas <-> panel memory layout
  EPD_Driver/           Elecrow's low-level EPD driver, vendored
web/                    the board's web pages (HTML/CSS)
test/                   native tests + parity fixtures
tools/                  generators, fixture maker, mockups, screen grabber
```

To add a setting: a member in `lib/settings/src/settings.h`, a row in
`SETTINGS_FIELDS` (settings.cpp), an input in `web/settings.html`. Storage,
the form and the JSON follow from the table.

Also: `examples/refresh_timing.cpp` (the refresh-timing test that produced the
numbers above), `vendor_reference/` (Elecrow's original sample files),
`lib/EPD_Driver/` is from
[Elecrow-RD/ESP32_S3-Ink-Screen](https://github.com/Elecrow-RD/ESP32_S3-Ink-Screen).

## Status

Building paper-planes. Phase 1 (server endpoint) and phase 2
(firmware: Wi-Fi, fetch, layout, refresh) done; phase 3 (airline logos)
done and checked on the panel. Phase 4 (stand-alone: lookups on the board,
web page at flights.local, night hours) done. Phase 5 (restructure: testable
libraries, parity tests, keep-alive, airport partition, PSRAM) done.
