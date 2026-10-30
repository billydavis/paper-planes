# Nearest-flight e-paper display — plan

Goal: the panel shows the aircraft closest to home: airline (with logo), route,
aircraft type, and live altitude / speed / distance / track. Built on the
`flight-tracker` project (`../flight-tracker`), which already does the hard
data work.

Mockups: `docs/mockups/` (rendered by `tools/mockup.py`, pixel-exact 792x272 1-bit).

## What we reuse from flight-tracker

| Piece | Where | Use here |
|---|---|---|
| Live positions near a point (adsb.lol, adsb.fi fallback) | `src/server/sources.py: nearby()` | find the nearest aircraft |
| Registration / type lookup (hexdb, adsbdb) | `enrich_aircraft()` | "Airbus A321neo · N421AN" |
| Callsign → route | `enrich_route()` | scheduled DFW → LAX |
| Real leg from the track | `deduce_leg()` | correct the route when a flight number is reused |
| Airport DB | `data/airports.csv`, `airports.py` | city names |
| ICAO prefix → airline | `src/web/assets/js/airlines.js` | "AAL…" → American, and which logo to show |
| Visual style ("printed aeronautical card", B612 fonts) | `DESIGN.md` | carries over to e-paper well |

## Architecture

**Recommended for phase 1: a small server does the data work, the board just displays.**

- Add an `/api/epaper/nearest?lat=&lon=` endpoint to flight-tracker (or a small
  script that imports `sources.py`). It picks the nearest airborne aircraft,
  enriches it, runs `deduce_leg`, and returns compact JSON (~300 bytes).
- The ESP32 polls it over Wi-Fi every ~10 s and draws the screen itself, with
  logos and fonts stored in its flash.

Why: it reuses the Python logic as-is (leg deduction especially would be a pain
to port), and it keeps the ESP32 code small. Downside: needs an always-on
machine (PC, Pi, or a cheap cloud function).

Alternative later: **standalone ESP32** calling adsb.lol and adsbdb directly
over HTTPS. Feasible (8 MB PSRAM handles the JSON; a filtered airport list fits
in flash), but means porting the leg logic to C++.

## Choosing "the closest flight"

- Home lat/lon set in config (the panel doesn't move). Search radius ~15 nm.
- Skip aircraft on the ground and ones with no position.
- Rank by ground distance (what flight-tracker does). Could also try slant range,
  which favours low planes you can actually see.
- Hysteresis: only switch to a new aircraft if it's closer by a margin (e.g.
  0.5 nm) or the current one is gone. Avoids flip-flopping and extra full refreshes.

## Screen layout (792 x 272)

- **Left 272 px:** logo square (224 px) + callsign underneath. No airline → a
  silhouette by aircraft category (airliner, small prop, helicopter, etc.).
- **Right:** airline name + flight number, big route codes with city names,
  aircraft type + registration, then a row of 4 stats with small indicators
  (climbing/level/descending arrow; arrow pointing toward the plane from you).
- Unknown destination → "Destination not yet known" instead of a code.
- **Idle screen** when nothing is in range ("Quiet skies", last aircraft seen,
  time). **Stale data marker** if the last good update is old, since e-paper
  keeps showing the last image even if the board dies.

## Refresh strategy (using our measured timings)

- New aircraft → full refresh (~2.1 s) to clear ghosting.
- Same aircraft, stats changed → partial refresh of the stats row only (~0.65 s).
- Force a full refresh every ~20 partials to keep the image clean.

## Logos

Findings from `docs/mockups/dither_compare_view.png`:

- **Threshold** (plain black/white cut) loses mid tones: whole stripes vanish.
- **Floyd-Steinberg** keeps tones but looks noisy on flat colours.
- **Atkinson** is crisper and handles gradients well. Best default for logos
  with shading.
- **Pattern map** (each tone snapped to a fixed fill: solid, 75%, 50% checker,
  25% dots, white) gives the cleanest result for **flat-colour** logos, which is
  most airline marks.
- At ~145 pixels per inch the dither dots are ~0.17 mm, so they read as grey
  at arm's length.

Pipeline (offline, in Python, extending `tools/mockup.py`):

1. Source art per ICAO code (PNG/SVG, from the airline's press/brand kit; prefer
   the tail mark over the wordmark, since the name is already in text). We keep
   source art out of git.
2. Downscale (Lanczos) → stretch contrast → light sharpen.
3. Pattern map or Atkinson, chosen per logo; hand-tweak the airlines we see most.
4. Pack to 1 bit per pixel: 224x224 = 6.3 KB each, so ~80 airlines ≈ 500 KB in
   flash (or `/logos/AAL.bin` on the SD card).

Stretch idea: the SSD1683 controller can sometimes do 4 grey levels with a
custom waveform. Not verified for this panel, and greyscale refreshes are slow,
so treat it as an experiment only.

## Phases

1. **Data endpoint:** `/api/epaper/nearest` in flight-tracker, returning compact JSON. **Done** (not committed yet), see `docs/epaper-api.md`.
2. **Device basics:** Wi-Fi + config (`src/secrets.h`), fetch every 10 s with `sticky`,
   full/partial refresh rules, quiet-skies / no-Wi-Fi / no-server / stale screens,
   silhouettes per aircraft class. Fonts are generated from the mockup fonts, and
   the panel's frame (read back over serial) matches the mockups pixel for pixel.
   **Done** and running live. Plane changes use a wipe-to-white then draw sequence (~3.3 s), because a plain full refresh ghosted. Bahnschrift kept for now
   (B612 swap still open).
3. **Logos:** conversion pipeline + top ~15 US airlines, plus category silhouettes.
4. **Refresh polish:** partial updates, hysteresis, idle and stale screens.
5. **Extras:** rotary switch to step through the next-nearest aircraft; a mini
   radar view on MENU.
