#!/usr/bin/env python3
"""
Teeb käivitusekraani logo pildi püsivara jaoks: main/boot_logo.c (LVGL RGB565A8).

Sama kujundus nagu docs/logo.svg: "SIMPL" sinise ja "DASH" kollakaspunase üleminekuga,
kaldkirjas, font Barlow Condensed Bold. Joonistatakse otse fondi kontuuridest
(lisateeke pole vaja). Käivita uuesti, kui muudad logo:

    python3 tools/logo/make_boot_logo.py
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fontpath as F  # noqa: E402

CAP = 104                      # suurtähe kõrgus pikslites
TRACK = 2.5                    # tähevahe (px)
GAP = 10                       # SIMPL ja DASH vahe (px)
SKEW = math.tan(math.radians(12))
PAD = 4
SS = 4                         # vertikaalne ülediskreetimine (silumine)
COL_SIMPL = ((0x00, 0xE5, 0xFF), (0x0A, 0x6C, 0xFF))   # ülal → all
COL_DASH = ((0xFF, 0xD2, 0x1F), (0xFF, 0x2D, 0x2D))
OUT = os.path.join(HERE, "..", "..", "main", "boot_logo.c")


def flatten(contour, steps=10):
    """TrueType kontuur (on/off punktid) → polügoon."""
    n = len(contour)
    st = next((i for i in range(n) if contour[i][2]), None)
    if st is None:
        a, b = contour[0], contour[1]
        start = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, 1)
        pts = contour
    else:
        start = contour[st]
        pts = contour[st + 1:] + contour[:st]
    out = [(start[0], start[1])]
    cur = (start[0], start[1])
    off = None

    def quad(p0, c, p1):
        for k in range(1, steps + 1):
            t = k / steps
            out.append(((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * c[0] + t * t * p1[0],
                        (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * c[1] + t * t * p1[1]))

    for q in pts + [start]:
        if q[2]:
            if off:
                quad(cur, off, (q[0], q[1]))
            else:
                out.append((q[0], q[1]))
            cur = (q[0], q[1])
            off = None
        else:
            if off:
                mid = ((off[0] + q[0]) / 2, (off[1] + q[1]) / 2)
                quad(cur, off, mid)
                cur = mid
            off = (q[0], q[1])
    return out


def word(text, x0, base):
    """Sõna polügoonid pikslites (vasak tindiserv x0, alusjoon base) ja laius."""
    k = CAP / F.CAP
    gl = [F.gid(c) for c in text]
    outs = [F.outline(g) for g in gl]
    ink_l = outs[0][1][0]
    polys, pos = [], 0.0
    for g, (cont, _) in zip(gl, outs):
        for c in cont:
            polys.append([(x0 + (pos + x - ink_l) * k, base - y * k) for x, y in flatten(c)])
        pos += F.adv(g) + TRACK / k
    last_pos = pos - F.adv(gl[-1]) - TRACK / k
    width = (last_pos + outs[-1][1][2] - ink_l) * k
    return polys, width


def raster(polys, w, h):
    """Nonzero täitmine, horisontaalselt täpne kate, vertikaalselt SS alamrida."""
    cov = [0.0] * (w * h)
    edges = []
    for poly in polys:
        for i in range(len(poly)):
            x0, y0 = poly[i]
            x1, y1 = poly[(i + 1) % len(poly)]
            if y0 != y1:
                edges.append((x0, y0, x1, y1, 1 if y1 > y0 else -1))
    for row in range(h):
        for j in range(SS):
            ys = row + (j + 0.5) / SS
            xs = []
            for x0, y0, x1, y1, d in edges:
                if (y0 <= ys < y1) or (y1 <= ys < y0):
                    xs.append((x0 + (ys - y0) * (x1 - x0) / (y1 - y0), d))
            xs.sort()
            wind = 0
            for i in range(len(xs) - 1):
                wind += xs[i][1]
                if wind == 0:
                    continue
                xa, xb = max(0.0, xs[i][0]), min(float(w), xs[i + 1][0])
                if xb <= xa:
                    continue
                ia, ib = int(xa), int(xb)
                base = row * w
                if ia == ib:
                    cov[base + ia] += (xb - xa) / SS
                else:
                    cov[base + ia] += (ia + 1 - xa) / SS
                    for x in range(ia + 1, min(ib, w)):
                        cov[base + x] += 1.0 / SS
                    if ib < w:
                        cov[base + ib] += (xb - ib) / SS
    return cov


def main():
    _, w1 = word("SIMPL", 0, 0)
    _, w2 = word("DASH", 0, 0)
    slant = CAP * SKEW
    W = int(math.ceil(w1 + GAP + w2 + slant)) + 2 * PAD
    H = CAP + 2 * PAD + 4
    base = PAD + CAP + 2
    p1, _ = word("SIMPL", PAD, base)
    p2, _ = word("DASH", PAD + w1 + GAP, base)

    def sk(polys):  # kaldkiri: ülemine serv paremale
        return [[(x + (base - y) * SKEW, y) for x, y in poly] for poly in polys]

    c1, c2 = raster(sk(p1), W, H), raster(sk(p2), W, H)
    rgb, alpha = bytearray(), bytearray()
    for y in range(H):
        t = min(1.0, max(0.0, (y - (base - CAP)) / CAP))
        g1 = [a + (b - a) * t for a, b in zip(*COL_SIMPL)]
        g2 = [a + (b - a) * t for a, b in zip(*COL_DASH)]
        for x in range(W):
            a1, a2 = min(1.0, c1[y * W + x]), min(1.0, c2[y * W + x])
            a = min(1.0, a1 + a2)
            col = g1 if a1 >= a2 else g2
            r, g, b = (int(v + 0.5) for v in col)
            v565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            rgb += bytes((v565 & 0xFF, v565 >> 8))
            alpha.append(int(a * 255 + 0.5))
    data = rgb + alpha

    with open(OUT, "w") as f:
        f.write("/* Genereeritud: tools/logo/make_boot_logo.py – ära muuda käsitsi. */\n")
        f.write("/* Font Barlow Condensed Bold (SIL OFL). */\n#include \"lvgl.h\"\n\n")
        f.write("static const uint8_t boot_logo_map[] = {\n")
        for i in range(0, len(data), 24):
            f.write("    " + ",".join("0x%02x" % b for b in data[i:i + 24]) + ",\n")
        f.write("};\n\n")
        f.write("const lv_image_dsc_t boot_logo = {\n")
        f.write("    .header = {\n")
        f.write("        .magic = LV_IMAGE_HEADER_MAGIC,\n")
        f.write("        .cf = LV_COLOR_FORMAT_RGB565A8,\n")
        f.write("        .w = %d,\n        .h = %d,\n        .stride = %d,\n" % (W, H, W * 2))
        f.write("    },\n")
        f.write("    .data_size = sizeof(boot_logo_map),\n")
        f.write("    .data = boot_logo_map,\n};\n")
    print("boot_logo.c: %dx%d, %d baiti" % (W, H, len(data)))


if __name__ == "__main__":
    main()
