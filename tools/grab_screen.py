"""Read the panel's current frame back from the board over USB serial.

    python tools/grab_screen.py NAME [--port COM5] [--next] [--compare MOCKUP_1BIT.png]

Writes docs/screens/NAME_1bit.png (exact pixels) and NAME_view.png (2x, paper
colours). --next first asks the firmware for its next demo sample and waits for
the refresh. --compare writes NAME_diff.png: red = only on the board,
blue = only in the mockup.
"""
import argparse
import os
import sys
import time

import serial
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mockup import view  # noqa: E402

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "screens")


def open_port(port):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 1
    s.dtr = s.rts = False          # don't reset the board when the port opens
    s.open()
    return s


def grab(s):
    s.reset_input_buffer()
    s.write(b"d")
    rows, w, h, deadline = [], 0, 0, time.time() + 10
    while time.time() < deadline:
        line = s.readline().decode("ascii", "replace").strip()
        if line.startswith("BEGIN CANVAS"):
            _, _, w, h = line.split()
            w, h = int(w), int(h)
            rows = []
        elif line == "END CANVAS":
            break
        elif w and len(line) == w // 4:
            rows.append(bytes.fromhex(line))
    if not w or len(rows) != h:
        raise SystemExit(f"no complete frame received ({len(rows)} rows)")
    img = Image.new("1", (w, h), 1)
    px = img.load()
    for y, row in enumerate(rows):
        for x in range(w):
            if row[x >> 3] & (0x80 >> (x & 7)):
                px[x, y] = 0
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--port", default="COM5")
    ap.add_argument("--next", action="store_true")
    ap.add_argument("--compare")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    with open_port(a.port) as s:
        if a.next:
            s.write(b"n")
            t0 = time.time()
            while time.time() - t0 < 15:     # wait for the refresh to finish
                line = s.readline().decode("ascii", "replace").strip()
                if line:
                    print("  board:", line)
                if line.startswith("[display] full"):
                    break
        img = grab(s)
    img.save(os.path.join(OUT, f"{a.name}_1bit.png"))
    view(img).save(os.path.join(OUT, f"{a.name}_view.png"))
    print("saved", os.path.join("docs", "screens", f"{a.name}_view.png"))
    if a.compare:
        ref = Image.open(a.compare).convert("1")
        diff = Image.new("RGB", img.size, (255, 255, 255))
        dp, bp, rp = diff.load(), img.load(), ref.load()
        n = 0
        for y in range(img.height):
            for x in range(img.width):
                b, r = bp[x, y] == 0, rp[x, y] == 0
                if b and r:
                    dp[x, y] = (160, 160, 160)
                elif b != r:
                    dp[x, y] = (220, 40, 40) if b else (40, 90, 220)
                    n += 1
        diff.save(os.path.join(OUT, f"{a.name}_diff.png"))
        print(f"{n} pixels differ from {os.path.basename(a.compare)}")


if __name__ == "__main__":
    main()
