"""Fetch airline logos and turn them into 1-bit bitmaps for the firmware.

    python tools/logogen.py [--airlines PATH] [--size 200] [--offline]

Writes src/generated/logos_gen.h and logos/preview.png (every logo as it will
look on the panel).

Logos come from github.com/Jxck-S/airline-logos, named by ICAO operator code
(the same 3-letter code that opens a callsign and keys flight-tracker's
airlines.js). Each one is downloaded once into logos/raw/ and reused after
that. To use your own artwork for an airline, drop logos/custom/<ICAO>.png in;
it wins over the downloaded one.

logos/ and the generated header are git-ignored: the logos are the airlines'
trademarks, fine on your own display but not ours to redistribute.
"""
import argparse
import os
import re
import sys
import urllib.request

from PIL import Image, ImageFilter, ImageOps

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
OUT = os.path.join(ROOT, "src", "generated", "logos_gen.h")
LOGOS = os.path.join(ROOT, "logos")
RAW = os.path.join(LOGOS, "raw")
CUSTOM = os.path.join(LOGOS, "custom")
DEFAULT_AIRLINES = os.path.join(ROOT, "..", "flight-tracker", "src", "web", "assets", "js", "airlines.js")

REPO = "https://raw.githubusercontent.com/Jxck-S/airline-logos/main/"
# Tried in order. FlightAware's are 90x90 symbol marks; RadarBox's are smaller
# (54x54) but cover a few airlines FlightAware lacks.
SOURCES = ["flightaware_logos", "radarbox_logos"]

# Operators worth a logo that aren't in airlines.js (no airline name on the
# display, but the callsign prefix still finds the logo).
EXTRA_CODES = ["EJA", "LXJ", "XOJ", "JRE", "AIP"]


def codes_from(path):
    with open(path, encoding="utf-8") as f:
        return re.findall(r"^\s+([A-Z0-9]{3}):", f.read(), re.M)


def fetch(code, offline):
    """Path of the source image for `code`, downloading it if needed."""
    custom = os.path.join(CUSTOM, code + ".png")
    if os.path.exists(custom):
        return custom, "custom"
    for src in SOURCES:
        path = os.path.join(RAW, f"{code}.{src}.png")
        if os.path.exists(path):
            return path, src
    if offline:
        return None, None
    for src in SOURCES:
        try:
            with urllib.request.urlopen(f"{REPO}{src}/{code}.png", timeout=15) as r:
                data = r.read()
        except Exception:
            continue
        path = os.path.join(RAW, f"{code}.{src}.png")
        with open(path, "wb") as f:
            f.write(data)
        return path, src
    return None, None


def patterns(gray):
    """Snap each tone to one of five fixed fill patterns (same as
    tools/mockup.py). Flat brand colours come out as even textures instead of
    dither noise. Returns a 1-bit image, black = 0."""
    w, h = gray.size
    src = gray.load()
    out = Image.new("1", (w, h), 1)
    op = out.load()
    for y in range(h):
        for x in range(w):
            v = src[x, y]
            if v < 60:
                on = True
            elif v < 120:
                on = not (x % 2 == 0 and y % 2 == 0)
            elif v < 180:
                on = (x + y) % 2 == 0
            elif v < 235:
                on = x % 2 == 0 and y % 2 == 0
            else:
                on = False
            op[x, y] = 0 if on else 1
    return out


def convert(path, size):
    """Source image -> 1-bit logo fitted inside size x size, cropped to ink."""
    img = Image.open(path).convert("RGBA")
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    img = Image.alpha_composite(bg, img).convert("L")
    # Trim the white margin first so the mark fills the box.
    box = ImageOps.invert(img).point(lambda v: 255 if v > 12 else 0).getbbox()
    if box:
        img = img.crop(box)
    scale = size / max(img.size)
    w, h = max(1, round(img.width * scale)), max(1, round(img.height * scale))
    img = img.resize((w, h), Image.Resampling.LANCZOS)
    img = ImageOps.autocontrast(img, cutoff=1)
    img = img.filter(ImageFilter.UnsharpMask(radius=1.5, percent=90, threshold=2))
    bw = patterns(img)
    box = ImageOps.invert(bw.convert("L")).getbbox()
    return bw.crop(box) if box else bw


def pack(bw):
    """Row-major, MSB first, rows padded to whole bytes, 1 = black."""
    w, h = bw.size
    px = bw.load()
    stride = (w + 7) // 8
    out = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            if px[x, y] == 0:
                out[y * stride + (x >> 3)] |= 0x80 >> (x & 7)
    return bytes(out)


def preview(logos, size):
    cell, cols = size + 30, 8
    rows = (len(logos) + cols - 1) // cols
    sheet = Image.new("L", (cols * cell, max(1, rows) * cell), 255)
    from PIL import ImageDraw
    d = ImageDraw.Draw(sheet)
    for i, (code, bw, _) in enumerate(logos):
        x, y = (i % cols) * cell, (i // cols) * cell
        sheet.paste(bw.convert("L"), (x + (cell - bw.width) // 2, y + 4 + (size - bw.height) // 2))
        d.text((x + 4, y + cell - 18), code, fill=0)
    sheet.save(os.path.join(LOGOS, "preview.png"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--airlines", default=DEFAULT_AIRLINES, help="flight-tracker airlines.js")
    ap.add_argument("--size", type=int, default=200, help="fit logos in a size x size box")
    ap.add_argument("--offline", action="store_true", help="use only logos already downloaded")
    a = ap.parse_args()
    os.makedirs(RAW, exist_ok=True)
    os.makedirs(CUSTOM, exist_ok=True)

    codes = codes_from(a.airlines) if os.path.exists(a.airlines) else []
    if not codes:
        print(f"logogen: {a.airlines} not found; using logos already in {RAW}")
    have = {f.split(".")[0] for f in os.listdir(RAW) + os.listdir(CUSTOM) if f.endswith(".png")}
    codes = sorted(set(codes) | set(EXTRA_CODES) | have)

    logos, missing = [], []
    for code in codes:
        path, src = fetch(code, a.offline)
        if not path:
            missing.append(code)
            continue
        try:
            bw = convert(path, a.size)
        except Exception as e:
            print(f"logogen: {code}: {e}")
            missing.append(code)
            continue
        logos.append((code, bw, src))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    total = 0
    with open(OUT, "w", newline="\n") as f:
        f.write("// Generated by tools/logogen.py - do not edit.\n#pragma once\n#include \"ui/logo.h\"\n\n")
        for code, bw, src in logos:
            data = pack(bw)
            total += len(data)
            f.write(f"// {code}: {bw.width}x{bw.height} from {src}\n")
            f.write(f"static const uint8_t LOGO_{code}[] = {{\n")
            for i in range(0, len(data), 24):
                f.write("  " + ",".join(f"0x{b:02x}" for b in data[i:i + 24]) + ",\n")
            f.write("};\n")
        f.write("\nstatic const Logo LOGOS[] = {\n")
        for code, bw, _ in logos:
            f.write(f'  {{"{code}", {bw.width}, {bw.height}, LOGO_{code}}},\n')
        f.write("};\nstatic const int NLOGOS = sizeof(LOGOS) / sizeof(LOGOS[0]);\n")
    if logos:
        preview(logos, a.size)
    print(f"logogen: {len(logos)} logos, {total / 1024:.0f} KB -> {os.path.relpath(OUT, ROOT)}")
    if missing:
        print("logogen: no logo for " + " ".join(missing))


if __name__ == "__main__":
    sys.exit(main())
