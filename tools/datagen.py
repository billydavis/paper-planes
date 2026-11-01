"""Build the data the firmware looks things up in.

    python tools/datagen.py [--flight-tracker PATH]

From flight-tracker's own data, so the board and the web app agree:
  data/airports.csv              (OurAirports) -> src/generated/airports.bin
  src/web/assets/js/airlines.js  (ICAO code -> airline name)
  src/server/epaper.py           (IATA dict)   -> src/generated/airlines_gen.h
and from Python's certifi plus tools/extra_roots.pem:
  root certificates -> src/generated/certs_gen.h (needs `pip install certifi cryptography`)

Without a flight-tracker checkout (FLIGHT_TRACKER_DIR, default ../flight-tracker)
the airline list comes from tools/airlines_snapshot.json (refreshed here
whenever flight-tracker is available) and airports.csv is downloaded from
OurAirports, the same file flight-tracker downloads.

airports.bin is the image lib/flights/src/airports.h describes: every open
large/medium/small airport (no heliports, seaplane bases or balloon ports,
like the server's nearest_airport()), sorted by latitude so a search can jump
straight to the band it needs, an index sorted by ICAO code for route lookups
("KDFW-KJAX" from hexdb), and a CRC so the board can tell a good image from
a half-written one. It goes in its own flash partition (partitions.csv).
"""
import argparse
import ast
import csv
import json
import os
import re
import struct
import sys
import unicodedata
import urllib.request
import zlib

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
GEN = os.path.join(ROOT, "src", "generated")
CACHE = os.path.join(ROOT, ".pio", "datagen")
SNAPSHOT = os.path.join(ROOT, "tools", "airlines_snapshot.json")
DEFAULT_FT = os.environ.get("FLIGHT_TRACKER_DIR") or os.path.join(ROOT, "..", "flight-tracker")
AIRPORTS_URL = "https://davidmegginson.github.io/ourairports-data/airports.csv"

TYPES = {"large_airport": 0, "medium_airport": 1, "small_airport": 2}
IMAGE_VERSION = 1   # lib/flights/src/airports.h AIRPORT_IMAGE_VERSION
HEADER = struct.Struct("<4s8I")   # AirportImageHeader


def ascii_text(s):
    """Same folding as epaper.py: "Aeroméxico" -> "Aeromexico"."""
    return unicodedata.normalize("NFKD", s or "").encode("ascii", "ignore").decode("ascii")


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


# ---- airports ---------------------------------------------------------------------
def airports_csv(ft):
    path = os.path.join(ft, "data", "airports.csv")
    if os.path.exists(path):
        return path
    path = os.path.join(CACHE, "airports.csv")
    if not os.path.exists(path):
        print(f"datagen: no flight-tracker checkout; downloading {AIRPORTS_URL}")
        os.makedirs(CACHE, exist_ok=True)
        urllib.request.urlretrieve(AIRPORTS_URL, path)
    return path


def airports(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if r["type"] not in TYPES or not r["latitude_deg"]:
                continue
            key = (r["icao_code"] or r["ident"]).upper()   # what the server shows
            iata = r["iata_code"].upper()
            if len(key) > 7 or len(iata) > 3 or not key.isascii():
                continue
            city = ascii_text(r["municipality"])[:39]
            rows.append((round(float(r["latitude_deg"]) * 1e6), round(float(r["longitude_deg"]) * 1e6),
                         TYPES[r["type"]], key, iata, city))
    rows.sort(key=lambda a: a[0])
    return rows


def airport_image(rows):
    pool, offs, seen = bytearray(), [], {}
    for _, _, _, key, iata, city in rows:
        entry = f"{key}\0{iata}\0{city}\0".encode("ascii")
        if entry not in seen:          # a few airports share code+city exactly
            seen[entry] = len(pool)
            pool += entry
        offs.append(seen[entry])
    count = len(rows)
    assert count < 65536
    records = b"".join(struct.pack("<iiI", lat, lon, typ | (off << 2))
                       for (lat, lon, typ, *_), off in zip(rows, offs))
    by_key = sorted(range(count), key=lambda i: rows[i][3])
    index = struct.pack(f"<{count}H", *by_key)
    if len(index) % 4:
        index += b"\0" * (4 - len(index) % 4)
    rec_off = HEADER.size + (-HEADER.size % 4)
    idx_off = rec_off + len(records)
    pool_off = idx_off + len(index)
    total = pool_off + len(pool)
    body = b"\0" * (rec_off - HEADER.size) + records + index + bytes(pool)
    crc = zlib.crc32(body) & 0xFFFFFFFF
    return HEADER.pack(b"APT1", IMAGE_VERSION, count, rec_off, idx_off, pool_off, len(pool), total, crc) + body


def write_airports(ft):
    rows = airports(airports_csv(ft))
    image = airport_image(rows)
    out = os.path.join(GEN, "airports.bin")
    with open(out, "wb") as f:
        f.write(image)
    print(f"datagen: {len(rows)} airports -> {os.path.relpath(out, ROOT)} ({len(image) // 1024} KB)")


# ---- airlines -----------------------------------------------------------------------
def airlines_from_flight_tracker(ft):
    js = os.path.join(ft, "src", "web", "assets", "js", "airlines.js")
    py = os.path.join(ft, "src", "server", "epaper.py")
    if not (os.path.exists(js) and os.path.exists(py)):
        return None
    with open(js, encoding="utf-8") as f:
        names = dict(re.findall(r'^\s*([A-Z]{3}):\s*\{\s*name:\s*"([^"]+)"', f.read(), re.M))
    iata = {}
    with open(py, encoding="utf-8") as f:
        for node in ast.walk(ast.parse(f.read())):
            if isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "IATA" for t in node.targets):
                iata = ast.literal_eval(node.value)
    return {code: {"name": ascii_text(names[code]), "iata": iata.get(code, "")} for code in sorted(names)}


def write_airlines(ft):
    table = airlines_from_flight_tracker(ft)
    if table is not None:
        # Only rewrite when the data changed: the checkout may hold CRLF line
        # endings (core.autocrlf), and an LF rewrite would make git, and the
        # build's version string, report the tree as dirty.
        snapshot = {"source": "flight-tracker airlines.js + epaper.py IATA", "airlines": table}
        try:
            with open(SNAPSHOT, encoding="utf-8") as f:
                unchanged = json.load(f) == snapshot
        except (OSError, ValueError):
            unchanged = False
        if not unchanged:
            with open(SNAPSHOT, "w", encoding="utf-8", newline="\n") as f:
                json.dump(snapshot, f, indent=1, sort_keys=True)
                f.write("\n")
    else:
        print(f"datagen: no flight-tracker checkout; using {os.path.relpath(SNAPSHOT, ROOT)}")
        with open(SNAPSHOT, encoding="utf-8") as f:
            table = json.load(f)["airlines"]
    out = os.path.join(GEN, "airlines_gen.h")
    with open(out, "w", encoding="ascii", newline="\n") as f:
        f.write("// Generated by tools/datagen.py from flight-tracker's airlines.js and epaper.py. Do not edit.\n")
        f.write('#pragma once\n#include "airlines.h"\n\n')
        f.write("static const AirlineRec AIRLINES[] = {  // sorted by ICAO code\n")
        for code in sorted(table):
            a = table[code]
            f.write(f'  {{"{code}", "{a["iata"]}", {c_str(a["name"])}}},\n')
        f.write("};\n")
        f.write("static const int AIRLINE_COUNT = sizeof(AIRLINES) / sizeof(AIRLINES[0]);\n")
    print(f"datagen: {len(table)} airlines -> {os.path.relpath(out, ROOT)}")


# ---- root certificates -------------------------------------------------------------------
def write_certs():
    """esp_crt_bundle format: u16 count, then per root (sorted by subject DER):
    u16 subject length, u16 public key length, subject DER, SubjectPublicKeyInfo DER."""
    try:
        import certifi
        from cryptography import x509
        from cryptography.hazmat.primitives import serialization
    except ImportError:
        sys.exit("datagen: needs certifi and cryptography (pip install certifi cryptography)")
    import warnings
    warnings.filterwarnings("ignore")   # a few old roots have odd serial numbers
    with open(certifi.where(), "rb") as f:
        pem = f.read()
    with open(os.path.join(ROOT, "tools", "extra_roots.pem"), "rb") as f:
        pem += f.read()
    roots = []
    for block in re.findall(rb"-----BEGIN CERTIFICATE-----.+?-----END CERTIFICATE-----", pem, re.S):
        c = x509.load_pem_x509_certificate(block)
        key = c.public_key().public_bytes(serialization.Encoding.DER,
                                          serialization.PublicFormat.SubjectPublicKeyInfo)
        roots.append((c.subject.public_bytes(), key))
    roots.sort(key=lambda r: r[0])
    blob = bytearray(len(roots).to_bytes(2, "big"))
    for name, key in roots:
        blob += len(name).to_bytes(2, "big") + len(key).to_bytes(2, "big") + name + key
    out = os.path.join(GEN, "certs_gen.h")
    with open(out, "w", encoding="ascii", newline="\n") as f:
        f.write("// Generated by tools/datagen.py from certifi's root certificates. Do not edit.\n")
        f.write("#pragma once\n#include <stdint.h>\n\nstatic const uint8_t CERT_BUNDLE[] = {\n")
        for i in range(0, len(blob), 32):
            f.write(",".join(str(b) for b in blob[i:i + 32]) + ",\n")
        f.write("};\n")
    print(f"datagen: {len(roots)} root certificates -> {os.path.relpath(out, ROOT)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--flight-tracker", default=DEFAULT_FT, help="path to the flight-tracker checkout")
    a = ap.parse_args()
    os.makedirs(GEN, exist_ok=True)
    write_airports(a.flight_tracker)
    write_airlines(a.flight_tracker)
    write_certs()


if __name__ == "__main__":
    main()
