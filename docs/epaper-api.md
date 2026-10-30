# `/api/epaper/nearest` — what the board fetches

Served by the flight-tracker project (`../flight-tracker`, code in
`src/server/epaper.py`). One request returns everything the screen needs.

## Running the server

```bash
cd ../flight-tracker
FT_HOST=0.0.0.0 python run.py        # PowerShell: $env:FT_HOST="0.0.0.0"; python run.py
```

`FT_HOST=0.0.0.0` makes it reachable from the board on your Wi-Fi (default is
localhost only). Windows may ask to allow Python through the firewall.

## Request

```
GET http://<pc-ip>:8000/api/epaper/nearest?lat=38.95&lon=-77.45&radius=15&sticky=a2af23
```

| Param | Required | Meaning |
|---|---|---|
| `lat`, `lon` | yes | home location |
| `radius` | no | search radius in nm, default 15, max 100 |
| `sticky` | no | `hex` the board is showing now. Kept unless another aircraft is 0.5 nm+ closer, so the screen doesn't flip between two planes at similar distance |

Suggested poll interval: 10 s. Upstream lookups are cached on the server, so a
warm request takes ~0.5 s.

## Responses

`200` with `"status": "none"` when nothing airborne is in range:

```json
{"status": "none", "radius_nm": 15.0, "updated": 1793396426}
```

`200` with `"status": "ok"`:

```json
{
  "status": "ok",
  "updated": 1793396425,          // server time, unix seconds
  "hex": "a2af23",                // ICAO address; send back as `sticky`
  "callsign": "ENY3668",          // as transmitted, or null
  "flight": "MQ 3668",            // IATA-style when the airline is known, else the callsign
  "airline": {"icao": "ENY", "iata": "MQ", "name": "Envoy Air"},  // or null (private, unknown)
  "reg": "N272NN",
  "type_code": "E75L",            // ICAO type designator
  "type_name": "Embraer EMB-175 LR",
  "category": "A3",               // ADS-B emitter category
  "shape": "narrowbody",          // narrowbody | widebody | ga | heli  (silhouette when no logo)
  "route": {
    "from": {"code": "DFW", "city": "Dallas-Fort Worth"},
    "to":   {"code": "JAX", "city": "Jacksonville"},   // or null = not yet known
    "basis": "track"
  },                              // or null when nothing is known
  "alt_ft": 500,                  // >= 0, or null
  "vrate_fpm": 2048,
  "vtrend": "up",                 // up | down | level
  "gs_kt": 164,
  "track_deg": 181,
  "dist_nm": 0.6,                 // from home
  "bearing_deg": 123,             // from home to the aircraft
  "compass": "SE",
  "squawk": "3654",
  "emergency": false,             // 7500/7600/7700 or an emergency flag
  "seen_s": 0.8,                  // age of the position, seconds
  "in_range": 16,                 // airborne aircraft within radius
  "radius_nm": 10.0
}
```

`route.basis` is how much to trust the route:

| basis | meaning | display idea |
|---|---|---|
| `confirmed` | schedule, and the observed origin matches | show normally |
| `scheduled` | schedule, position and heading fit it | show normally |
| `track` | worked out from what the plane is actually doing (schedule missing or wrong) | show; `to` may be null → "Destination not yet known" |
| `mismatch` | schedule only, and it doesn't fit where the plane is | show smaller, or mark with "?" |

All text is plain ASCII (accents folded), so a bitmap font with just ASCII
characters is enough.

Errors: `400` for missing/non-numeric `lat`/`lon`; `502` when every ADS-B feed
failed (board should keep the last screen and show a stale marker).

## Known limits

- adsb.lol now puts its per-aircraft track history behind a bot-check page, so
  the track-based origin/destination logic currently gets nothing from it. The
  endpoint falls back to the live position: an aircraft low and climbing near
  an airport is recorded as departing it (remembered while the server keeps
  running), and the destination is guessed once it's low and descending.
  Planes already at cruise when the server first sees them will usually show
  the scheduled route, or `mismatch`.
- Regional carriers show as themselves (SkyWest, Envoy), not the brand they fly
  for (United Express, American Eagle).
- Airlines missing from flight-tracker's `airlines.js` (e.g. ITA Airways) come
  back with `airline: null` and the raw callsign.
