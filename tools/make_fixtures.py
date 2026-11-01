"""Make the parity test fixtures in test/fixtures/ from flight-tracker's own code.

    python tools/make_fixtures.py [--flight-tracker PATH] [--live]

Each fixture is one call to flight-tracker's epaper.nearest(): the upstream
replies it got (ADS-B feed, hexdb, adsbdb) and the answer it gave. The C++
test (test/test_parity) feeds the same replies through lib/flights and checks
it reaches the same answer, so the port can't quietly drift from the Python.

Synthetic scenarios (always): hand-made feeds that exercise each route rule
(departure seen, schedule confirmed / fits / mismatch, sticky, ground filter,
nothing in range). No network.

--live: also record real traffic around a few big airports (public places,
not your home). Needs the network; rerun when flight-tracker's rules change.
"""
import argparse
import importlib
import json
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "test", "fixtures")
DEFAULT_FT = os.environ.get("FLIGHT_TRACKER_DIR") or os.path.join(ROOT, "..", "flight-tracker")

# The feed fields lib/flights/src/route.cpp feedFilter() keeps.
FEED_FIELDS = ["hex", "flight", "r", "t", "lat", "lon", "alt_baro", "alt_geom", "baro_rate", "geom_rate",
               "gs", "track", "category", "squawk", "emergency", "seen_pos"]
LIVE_SPOTS = {   # name: (lat, lon) - airports, so there is traffic
    "atl": (33.6367, -84.4281), "ord": (41.9786, -87.9048), "dfw": (32.8968, -97.0380),
    "lax": (33.9425, -118.4081), "jfk": (40.6398, -73.7789), "dca": (38.8521, -77.0377),
}


def role(url):
    if "/v2/point/" in url or "adsb.fi" in url:
        return "feed"
    if "/data/traces/" in url:
        return "trace"
    if "hexdb.io/api/v1/aircraft" in url:
        return "hexdb_aircraft"
    if "adsbdb.com/v0/aircraft" in url:
        return "adsbdb_aircraft"
    if "adsbdb.com/v0/callsign" in url:
        return "adsbdb_callsign"
    if "hexdb.io/api/v1/route" in url:
        return "hexdb_route"
    return "other"


def load_ft(ft):
    sys.path.insert(0, os.path.abspath(ft))
    sources = importlib.import_module("src.server.sources")
    epaper = importlib.import_module("src.server.epaper")
    return sources, epaper


def reset(sources, epaper):
    epaper._cache.clear()
    sources._origins.clear()


def strip_feed(feed):
    out = dict(feed)
    for key in ("ac", "aircraft"):
        if isinstance(out.get(key), list):
            out[key] = [{k: a[k] for k in FEED_FIELDS if k in a} for a in out[key]]
    return {k: v for k, v in out.items() if k in ("ac", "aircraft")}


def expected(answer):
    return {k: v for k, v in answer.items() if k not in ("updated", "seen_s")}


def run(sources, epaper, home, sticky, fetch):
    """Call nearest() with fetch_json replaced by `fetch(role, url)`; return
    (replies by role, answer)."""
    replies = {}

    def fake(url, timeout=8):
        r = role(url)
        if r == "trace":   # adsb.lol's traces are behind a bot check: always unavailable
            raise RuntimeError("trace unavailable")
        try:
            val = fetch(r, url)
        except Exception:
            replies.setdefault(r, None)
            raise
        replies[r] = strip_feed(val) if r == "feed" else val
        return val

    reset(sources, epaper)
    real = sources.fetch_json
    sources.fetch_json = fake
    try:
        answer = epaper.nearest(home[0], home[1], home[2], sticky)
    finally:
        sources.fetch_json = real
    return replies, answer


def save(name, home, sticky, replies, answer, note):
    os.makedirs(OUT, exist_ok=True)
    doc = {"note": note, "home": {"lat": home[0], "lon": home[1], "radius": home[2]}, "sticky": sticky,
           "replies": replies, "expected": expected(answer)}
    path = os.path.join(OUT, name + ".json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=1, sort_keys=True)
        f.write("\n")
    route = answer.get("route") or {}
    print(f"  {name:28s} {answer.get('flight') or answer['status']:10s} {route.get('basis') or '-'}")


# ---- synthetic scenarios --------------------------------------------------------------
def ac(**kw):
    a = {"hex": "abc123", "flight": "DAL123  ", "lat": 33.66, "lon": -84.42, "alt_baro": 2000, "baro_rate": 1800,
         "gs": 180, "track": 180, "category": "A3", "squawk": "1234", "r": "N123DL", "t": "A321", "seen_pos": 0.4}
    a.update(kw)
    return {k: v for k, v in a.items() if v is not None}


def adsbdb_route(o, d):
    def p(code, iata, city, lat, lon):
        return {"icao_code": code, "iata_code": iata, "municipality": city, "name": city + " Airport",
                "latitude": lat, "longitude": lon}
    return {"response": {"flightroute": {"origin": p(*o), "destination": p(*d)}}}


ATL = ("KATL", "ATL", "Atlanta", 33.6367, -84.4281)
MCO = ("KMCO", "MCO", "Orlando", 28.4294, -81.3090)
BOS = ("KBOS", "BOS", "Boston", 42.3643, -71.0052)
SEA = ("KSEA", "SEA", "Seattle", 47.4490, -122.3093)
HEXDB_AC = {"Registration": "N123DL", "Type": "A321-211", "ICAOTypeCode": "A321", "Manufacturer": "Airbus"}

SCENARIOS = {
    # Low and climbing next to ATL with a schedule out of ATL: "confirmed".
    "syn_departure_confirmed": ((33.70, -84.40, 15), None,
                                {"feed": {"ac": [ac()]}, "hexdb_aircraft": HEXDB_AC,
                                 "adsbdb_callsign": adsbdb_route(ATL, MCO)}),
    # Climbing out of ATL, but the schedule says BOS -> SEA: trust the track.
    "syn_departure_track": ((33.70, -84.40, 15), None,
                            {"feed": {"ac": [ac()]}, "hexdb_aircraft": HEXDB_AC,
                             "adsbdb_callsign": adsbdb_route(BOS, SEA)}),
    # Cruising between ATL and MCO, heading for MCO: "scheduled".
    "syn_scheduled_fits": ((31.0, -82.9, 15), None,
                           {"feed": {"ac": [ac(lat=31.0, lon=-82.9, alt_baro=35000, baro_rate=0, track=150)]},
                            "hexdb_aircraft": HEXDB_AC, "adsbdb_callsign": adsbdb_route(ATL, MCO)}),
    # Same plane heading the wrong way for the schedule: "mismatch".
    "syn_mismatch": ((31.0, -82.9, 15), None,
                     {"feed": {"ac": [ac(lat=31.0, lon=-82.9, alt_baro=35000, baro_rate=0, track=330)]},
                      "hexdb_aircraft": HEXDB_AC, "adsbdb_callsign": adsbdb_route(ATL, MCO)}),
    # hexdb's route (ICAO codes, looked up in the airport table) when adsbdb has none.
    "syn_hexdb_route": ((31.0, -82.9, 15), None,
                        {"feed": {"ac": [ac(lat=31.0, lon=-82.9, alt_baro=35000, baro_rate=0, track=150)]},
                         "hexdb_aircraft": HEXDB_AC, "hexdb_route": {"route": "KATL-KMCO"}}),
    # Private plane, no route anywhere, adsbdb for the airframe.
    "syn_private_no_route": ((30.62, -97.63, 15), None,
                             {"feed": {"ac": [ac(hex="a1b2c3", flight="N172SP", category="A1", alt_baro=3500,
                                                 baro_rate=0, gs=105, lat=30.65, lon=-97.60, r="N172SP", t="C172")]},
                              "adsbdb_aircraft": {"response": {"aircraft": {
                                  "registration": "N172SP", "type": "172S Skyhawk SP",
                                  "manufacturer": "Cessna", "icao_type": "C172"}}}}),
    # Sticky: the plane on screen stays while another is < 0.5 nm closer.
    "syn_sticky": ((33.70, -84.40, 15), "bbb222",
                   {"feed": {"ac": [ac(hex="aaa111", flight="SWA1", lat=33.75, lon=-84.40, alt_baro=9000,
                                       baro_rate=0),
                                    ac(hex="bbb222", flight="UAL2", lat=33.70, lon=-84.466, alt_baro=9000,
                                       baro_rate=0)]}}),
    # Aircraft on the ground don't count; emergency squawk flagged.
    "syn_ground_and_emergency": ((33.70, -84.40, 15), None,
                                 {"feed": {"ac": [ac(hex="ccc333", lat=33.701, lon=-84.401, alt_baro="ground"),
                                                  ac(hex="ddd444", flight="N5", category="A1", squawk="7700",
                                                     alt_baro=4500, baro_rate=-200, lat=33.8, lon=-84.4)]}}),
    # Nothing in range.
    "syn_none": ((33.70, -84.40, 15), None, {"feed": {"ac": []}}),
}


def synthetic(sources, epaper):
    print("synthetic:")
    for name, (home, sticky, data) in SCENARIOS.items():
        def fetch(r, url, data=data):
            if r not in data:
                raise RuntimeError("no reply")
            return data[r]
        replies, answer = run(sources, epaper, home, sticky, fetch)
        save(name, home, sticky, replies, answer, "synthetic")


def live(sources, epaper, real_fetch):
    print("live:")
    for name, (lat, lon) in LIVE_SPOTS.items():
        replies, answer = run(sources, epaper, (lat, lon, 15), None, lambda r, url: real_fetch(url))
        if not replies.get("feed"):
            print(f"  live_{name}: feed failed, skipped")
            continue
        save("live_" + name, (lat, lon, 15), None, replies, answer, "recorded live traffic")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--flight-tracker", default=DEFAULT_FT)
    ap.add_argument("--live", action="store_true", help="also record live traffic (network)")
    a = ap.parse_args()
    sources, epaper = load_ft(a.flight_tracker)
    real_fetch = sources.fetch_json
    synthetic(sources, epaper)
    if a.live:
        live(sources, epaper, real_fetch)


if __name__ == "__main__":
    main()
