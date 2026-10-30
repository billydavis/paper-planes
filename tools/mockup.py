"""Render pixel-exact mockups of the nearest-flight screen for the 792x272
1-bit e-paper panel, plus a comparison of logo dithering methods.

    python tools/mockup.py            -> writes docs/mockups/*.png

Outputs come in two forms:
  *_1bit.png  - exactly what the panel would get (792x272, 1 bit per pixel)
  *_view.png  - the same pixels shown 2x on an e-paper-ish palette, for viewing

The logo here is a made-up stand-in (a tail fin with livery stripes), not a
real airline logo. It has flat tones, a gradient and thin lines, which are the
parts of a real logo that dithering struggles with.
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageOps

W, H = 792, 272
OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "mockups")
FONTS = "C:/Windows/Fonts/"
INK, PAPER = (34, 34, 36), (226, 224, 216)


# ---- fonts ----------------------------------------------------------------
def bahn(size, weight="Regular"):
    f = ImageFont.truetype(FONTS + "bahnschrift.ttf", size)
    f.set_variation_by_name(weight)
    return f


def mono(size, bold=True):
    return ImageFont.truetype(FONTS + ("consolab.ttf" if bold else "consola.ttf"), size)


def spaced(d, xy, text, font, track=0, fill=0, anchor_right=False):
    """Draw text with extra letter-spacing. Returns the drawn width."""
    widths = [d.textlength(c, font=font) for c in text]
    total = sum(widths) + track * (len(text) - 1)
    x, y = xy
    if anchor_right:
        x -= total
    for c, w in zip(text, widths):
        d.text((x, y), c, font=font, fill=fill)
        x += w + track
    return total


# ---- stand-in logo (grayscale) ---------------------------------------------
def standin_logo(size=960):
    img = Image.new("L", (size, size), 255)
    s = size / 960
    P = lambda pts: [(x * s, y * s) for x, y in pts]
    fin = P([(150, 900), (470, 90), (790, 90), (650, 900)])

    # Fin body: vertical gradient, light at the top, darker at the root.
    grad = Image.new("L", (size, size))
    gd = ImageDraw.Draw(grad)
    for y in range(size):
        gd.line([(0, y), (size, y)], fill=int(215 - 95 * y / size))
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).polygon(fin, fill=255)
    img.paste(grad, (0, 0), mask)

    # Livery stripes (think red / white / blue), clipped to the fin.
    stripes = Image.new("L", (size, size), 255)
    sd = ImageDraw.Draw(stripes)
    for off, tone, w in ((520, 40, 120), (640, 255, 60), (700, 105, 110)):
        sd.polygon(P([(0, off), (960, off - 420), (960, off - 420 + w), (0, off + w)]), fill=tone)
    smask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(smask).polygon(
        P([(0, 520), (960, 100), (960, 490), (0, 910)]), fill=255)
    img.paste(stripes, (0, 0), Image.composite(smask, Image.new("L", (size, size), 0), mask))

    d = ImageDraw.Draw(img)
    # Emblem: dark disc with a white swept wing mark and a thin ring.
    cx, cy, r = 590 * s, 300 * s, 130 * s
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=30)
    d.ellipse([cx - r + 18 * s, cy - r + 18 * s, cx + r - 18 * s, cy + r - 18 * s],
              outline=255, width=int(6 * s))
    d.polygon(P([(500, 340), (680, 230), (650, 280), (560, 330), (640, 330), (610, 360)]), fill=255)
    d.line(fin + [fin[0]], fill=20, width=int(10 * s))
    return img


# ---- dithering ------------------------------------------------------------
def threshold(img, t=128):
    return img.point(lambda v: 255 if v >= t else 0).convert("1", dither=Image.Dither.NONE)


def floyd(img):
    return img.convert("1")  # Pillow's default is Floyd-Steinberg


def atkinson(img):
    """Atkinson dither: spreads only 6/8 of the error, so flat areas stay clean
    and edges stay crisp. Tends to suit logos and line art better than F-S."""
    w, h = img.size
    src = img.load()
    px = [[float(src[x, y]) for x in range(w)] for y in range(h)]
    out = Image.new("1", (w, h), 1)
    op = out.load()
    for y in range(h):
        row = px[y]
        for x in range(w):
            old = row[x]
            new = 255.0 if old >= 128 else 0.0
            op[x, y] = 1 if new else 0
            e = (old - new) / 8
            for dx, dy in ((1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and ny < h:
                    px[ny][nx] += e
    return out


BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def ordered(img):
    w, h = img.size
    src = img.load()
    out = Image.new("1", (w, h), 1)
    op = out.load()
    for y in range(h):
        for x in range(w):
            op[x, y] = 1 if src[x, y] > (BAYER4[y % 4][x % 4] + 0.5) * 16 else 0
    return out


def patterns(img):
    """Snap each tone to one of five fixed fill patterns. Flat brand colours
    come out as clean, even textures instead of noise."""
    w, h = img.size
    src = img.load()
    out = Image.new("1", (w, h), 1)
    op = out.load()
    for y in range(h):
        for x in range(w):
            v = src[x, y]
            if v < 60:
                on = True                                    # solid
            elif v < 120:
                on = not (x % 2 == 0 and y % 2 == 0)         # 75%
            elif v < 180:
                on = (x + y) % 2 == 0                        # 50% checker
            elif v < 235:
                on = (x % 2 == 0 and y % 2 == 0)             # 25% dots
            else:
                on = False
            op[x, y] = 0 if on else 1
    return out


def prep(img, size):
    """Downscale with a good filter, stretch contrast, and sharpen a little so
    edges survive the trip to 1 bit."""
    small = img.resize((size, size), Image.Resampling.LANCZOS)
    small = ImageOps.autocontrast(small, cutoff=1)
    return small.filter(ImageFilter.UnsharpMask(radius=1.2, percent=80, threshold=2))


# ---- screen pieces ----------------------------------------------------------
def arrow(d, cx, cy, bearing, r=11):
    """Small filled arrow pointing along `bearing` (degrees, 0 = up/north)."""
    pts = [(0, -r), (r * 0.75, r), (0, r * 0.45), (-r * 0.75, r)]
    a = math.radians(bearing)
    rot = [(cx + x * math.cos(a) - y * math.sin(a), cy + x * math.sin(a) + y * math.cos(a))
           for x, y in pts]
    d.polygon(rot, fill=0)


def route_arrow(d, x0, x1, y):
    d.line([(x0, y), (x1 - 4, y)], fill=0, width=5)
    d.polygon([(x1, y), (x1 - 18, y - 12), (x1 - 18, y + 12)], fill=0)


def stats_row(d, x0, y, cells, cell_w):
    lab, val, unit = bahn(13, "Bold"), mono(30), bahn(16, "SemiBold")
    for i, (label, value, u, extra) in enumerate(cells):
        x = x0 + i * cell_w
        if i:
            d.line([(x - 10, y + 2), (x - 10, y + 50)], fill=0, width=1)
        lw = spaced(d, (x, y), label, lab, track=2)
        if extra:
            extra(d, x + lw + 14, y + 7)
        d.text((x, y + 18), value, font=val, fill=0)
        vw = d.textlength(value, font=val)
        if u:
            d.text((x + vw + 4, y + 30), u, font=unit, fill=0)


def plane_icon(size=200):
    """Top-down silhouette of a small high-wing piston plane."""
    img = Image.new("1", (size, size), 1)
    d = ImageDraw.Draw(img)
    s = size / 200
    S = lambda pts: [(x * s, y * s) for x, y in pts]
    d.rounded_rectangle(S([(92, 18), (108, 182)])[0] + S([(92, 18), (108, 182)])[1], radius=8 * s, fill=0)
    d.polygon(S([(12, 62), (188, 62), (188, 84), (12, 84)]), fill=0)          # wing
    d.polygon(S([(56, 160), (144, 160), (140, 176), (60, 176)]), fill=0)      # tailplane
    d.ellipse(S([(90, 10), (110, 30)])[0] + S([(90, 10), (110, 30)])[1], fill=0)   # nose
    return img


def screen(logo_1bit, airline, flight, frm, to, frm_city, to_city, aircraft,
           stats, caption):
    img = Image.new("1", (W, H), 1)
    d = ImageDraw.Draw(img)
    d.fontmode = "1"

    # Left: logo panel
    lw, lh = logo_1bit.size
    img.paste(logo_1bit, ((272 - lw) // 2, 14))
    cf = bahn(12, "Bold")
    cw = sum(d.textlength(c, font=cf) for c in caption) + 2 * (len(caption) - 1)
    spaced(d, (136 - cw / 2, 248), caption, cf, track=2)
    d.line([(272, 14), (272, 258)], fill=0, width=2)

    x0, xr = 292, 776
    # Header: airline + flight number
    spaced(d, (x0, 14), airline.upper(), bahn(28, "Bold"), track=4)
    spaced(d, (xr, 12), flight, mono(32), anchor_right=True)

    # Route
    big = bahn(84, "Bold")
    d.text((x0 - 4, 44), frm, font=big, fill=0)
    fw = d.textlength(frm, font=big)
    ax0, ax1 = x0 + fw + 18, x0 + fw + 96
    route_arrow(d, ax0, ax1, 88)
    city = bahn(18, "Regular")
    d.text((x0, 138), frm_city, font=city, fill=0)
    if to:
        d.text((ax1 + 18, 44), to, font=big, fill=0)
        d.text((ax1 + 22, 138), to_city, font=city, fill=0)
    else:
        spaced(d, (ax1 + 22, 72), "DESTINATION", bahn(15, "Bold"), track=3)
        spaced(d, (ax1 + 22, 92), "NOT YET KNOWN", bahn(15, "Bold"), track=3)

    # Aircraft line
    d.text((x0, 168), aircraft, font=bahn(22, "SemiBold"), fill=0)

    d.line([(x0, 202), (xr, 202)], fill=0, width=1)
    stats_row(d, x0, 210, stats, 124)
    return img


def view(img, scale=2):
    """1-bit image -> e-paper-ish RGB, upscaled with hard pixels."""
    rgb = Image.new("RGB", img.size)
    rgb.paste(PAPER, (0, 0, *img.size))
    rgb.paste(INK, (0, 0, *img.size), ImageOps.invert(img.convert("L")))
    return rgb.resize((img.width * scale, img.height * scale), Image.Resampling.NEAREST)


def save(img, name):
    img.save(os.path.join(OUT, f"{name}_1bit.png"))
    view(img).save(os.path.join(OUT, f"{name}_view.png"))


def main():
    os.makedirs(OUT, exist_ok=True)
    logo = standin_logo()
    small = prep(logo, 224)

    # 1. Dithering comparison sheet
    methods = [("THRESHOLD", threshold), ("FLOYD-STEINBERG", floyd),
               ("ATKINSON", atkinson), ("ORDERED 4x4", ordered), ("PATTERN MAP", patterns)]
    tile, gap = 224, 24
    sheet = Image.new("1", (gap + len(methods) * (tile + gap), tile + 70), 1)
    sd = ImageDraw.Draw(sheet)
    sd.fontmode = "1"
    for i, (name, fn) in enumerate(methods):
        x = gap + i * (tile + gap)
        sheet.paste(fn(small), (x, 16))
        spaced(sd, (x, tile + 30), name, bahn(15, "Bold"), track=2)
    save(sheet, "dither_compare")
    logo.resize((224, 224), Image.Resampling.LANCZOS).save(os.path.join(OUT, "standin_logo_gray.png"))

    # 2. Airline flight
    climb = lambda d, x, y: arrow(d, x, y, 0, r=7)
    bearing = lambda d, x, y: arrow(d, x, y, 45, r=7)
    s1 = screen(
        atkinson(small), "American", "AA 2417", "DFW", "LAX",
        "Dallas/Fort Worth", "Los Angeles", "Airbus A321neo  \u00b7  N421AN",
        [("ALTITUDE", "8,400", "ft", climb), ("SPEED", "286", "kt", None),
         ("DISTANCE", "3.2", "nm", bearing), ("TRACK", "263\u00b0", "", None)],
        "AAL2417")
    save(s1, "screen_airline")

    # 3. Private / GA aircraft (no airline logo)
    level = lambda d, x, y: d.line([(x - 7, y), (x + 7, y)], fill=0, width=3)
    s2 = screen(
        plane_icon(200), "Private", "N172SP", "GAI", None,
        "Montgomery County", "unknown", "Cessna 172S Skyhawk",
        [("ALTITUDE", "2,500", "ft", level), ("SPEED", "104", "kt", None),
         ("DISTANCE", "1.1", "nm", lambda d, x, y: arrow(d, x, y, 200, r=7)),
         ("TRACK", "018\u00b0", "", None)],
        "GA \u00b7 A1")
    save(s2, "screen_private")
    print("wrote", os.path.abspath(OUT))


if __name__ == "__main__":
    main()
