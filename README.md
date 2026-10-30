# CrowPanel 5.79" E-Paper (ESP32-S3) — exploration project

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
`C:\Users\billy\Documents\esp32_backups\crowpanel_579_full_8MB_backup_2026-10-26.bin`.

Restore with:
```
python -m esptool --port COM5 write-flash 0x0 crowpanel_579_full_8MB_backup_2026-10-26.bin
```

## Nearest-flight display (current firmware)

Shows the closest airborne aircraft, fed by the flight-tracker server's
`/api/epaper/nearest` endpoint (see `docs/epaper-api.md`). Plan and mockups:
`docs/flight-display-plan.md`, `docs/mockups/`.

Setup:
1. Edit `src/secrets.h`: Wi-Fi name/password, server URL, home lat/lon, time zone.
   With `WIFI_SSID` empty the board runs in **demo mode** (built-in sample flights).
2. On the PC, start the server so the board can reach it:
   `cd ../flight-tracker` then `$env:FT_HOST="0.0.0.0"; python run.py` (PowerShell).
3. Build and flash: `pio run -t upload`. The first build generates the fonts
   (`tools/fontgen.py`, needs Python + Pillow and the Windows fonts).

Refresh: when the aircraft changes (~3.3 s) the panel is wiped to white and
the new screen drawn from that clean state, which is Elecrow's own sequence. A
plain full refresh of an image left the previous screen ghosted. Partial
refresh (~0.9 s) when only the numbers change, and a forced clean redraw every
20 partials or 30 minutes.

Serial commands (115200): `d` dump screen, `f` force full refresh, `n` next demo flight.
`python tools/grab_screen.py NAME [--next] [--compare mockup_1bit.png]` saves the
panel's current frame to `docs/screens/` (and diffs it against a mockup).

## Layout

- `src/main.cpp` — Wi-Fi, polling, refresh decisions, demo mode, serial commands
- `src/screen.cpp` — screen layouts (coordinates follow `tools/mockup.py`)
- `src/canvas.cpp` — 792x272 1-bit drawing: text, lines, polygons, circles
- `src/display.cpp` — full/partial refresh on top of the vendor driver
- `src/flight.cpp` — parses the endpoint's JSON
- `src/secrets.h` — your settings (git-ignored); template in `secrets.example.h`
- `src/generated/fonts_gen.h` — bitmap fonts, generated at build time (git-ignored)
- `tools/` — `mockup.py` (mockups + dithering), `fontgen.py`, `grab_screen.py`
- `lib/EPD_Driver/` — vendored copy of Elecrow's official low-level EPD driver (bit-banged SPI, SSD1683 register sequences), from [Elecrow-RD/ESP32_S3-Ink-Screen](https://github.com/Elecrow-RD/ESP32_S3-Ink-Screen)
- `examples/refresh_timing.cpp` — the refresh-timing test that produced the numbers above
- `vendor_reference/` — original unmodified Elecrow sample files, kept for reference

## Status

Building the nearest-flight display. Phase 1 (server endpoint) and phase 2
(firmware: Wi-Fi, fetch, layout, refresh) done; phase 3 is airline logos.
