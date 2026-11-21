#!/usr/bin/env python3
"""
Teeb ANALOOG vaate osutite pildid: main/needles.c (LVGL ARGB8888) ja main/needles.h.

Kuju nagu 90ndate Jaapani sportauto kellades: punane, koonusjas, lühike "saba" üle telje,
pealt heledam ja alt tumedam (3D), pehmed servad. Pilt osutab paremale; telg on punktis
(pivot_x, h/2) – LVGL pöörab pilti ümber selle punkti.

    python3 tools/logo/make_needles.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from make_boot_logo import raster  # noqa: E402  (sama silutud polügoonide joonistaja)

OUT_C = os.path.join(HERE, "..", "..", "main", "needles.c")
OUT_H = os.path.join(HERE, "..", "..", "main", "needles.h")

LIGHT = (0xFF, 0x4A, 0x3A)    # ülemine serv
MID = (0xE8, 0x22, 0x1E)      # keskjoon
DARK = (0xA8, 0x10, 0x10)     # alumine serv

# nimi, tipu kaugus teljest, saba pikkus, laius: saba otsas, teljel, tipus
NEEDLES = [
    ("needle_big", 153, 38, 5.0, 6.5, 3.2),
    ("needle_small", 64, 26, 4.5, 7.5, 1.6),
]


def make(name, tip, tail, w_tail, w_piv, w_tip):
    pad = 2
    h = int(max(w_tail, w_piv) + 2 * pad + 1) | 1      # paaritu kõrgus → telg keskel
    w = tail + tip + 2 * pad
    cy = h / 2.0
    px = pad + tail                                      # telg pildis
    x0, x1 = pad, px + tip
    # kontuur: saba ots (ümardatud), telg, tipp (terav, kergelt ümar)
    top = [(x0 + 1.5, cy - w_tail / 2), (px, cy - w_piv / 2), (x1 - 1.0, cy - w_tip / 2), (x1, cy)]
    bot = [(x1 - 1.0, cy + w_tip / 2), (px, cy + w_piv / 2), (x0 + 1.5, cy + w_tail / 2), (x0, cy)]
    cov = raster([top + bot], w, h)
    data = bytearray()
    for y in range(h):
        t = (y + 0.5 - cy) / (w_piv / 2)                 # -1 ülal, +1 all
        t = max(-1.0, min(1.0, t))
        if t < 0:
            c = [m + (l - m) * (-t) for l, m in zip(LIGHT, MID)]
        else:
            c = [m + (d - m) * t for d, m in zip(DARK, MID)]
        for x in range(w):
            a = min(1.0, cov[y * w + x])
            b, g, r = int(c[2] + .5), int(c[1] + .5), int(c[0] + .5)
            data += bytes((b, g, r, int(a * 255 + .5)))   # ARGB8888 = B, G, R, A mälus
    return w, h, px, data


def main():
    c = ["/* Genereeritud: tools/logo/make_needles.py – ära muuda käsitsi. */", '#include "needles.h"', ""]
    hdr = ["/* Genereeritud: tools/logo/make_needles.py */", "#pragma once", '#include "lvgl.h"', ""]
    for name, *geom in NEEDLES:
        w, h, px, data = make(name, *geom)
        c.append("static const uint8_t %s_map[] = {" % name)
        for i in range(0, len(data), 24):
            c.append("    " + ",".join("0x%02x" % b for b in data[i:i + 24]) + ",")
        c.append("};")
        c.append("const lv_image_dsc_t %s = {" % name)
        c.append("    .header = { .magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_ARGB8888,")
        c.append("                .w = %d, .h = %d, .stride = %d }," % (w, h, w * 4))
        c.append("    .data_size = sizeof(%s_map)," % name)
        c.append("    .data = %s_map," % name)
        c.append("};")
        c.append("")
        hdr.append("extern const lv_image_dsc_t %s;" % name)
        hdr.append("#define %s_PIVOT_X %d   /* telje x pildis; y = h/2 */" % (name.upper(), px))
        print("%s: %dx%d, telg x=%d" % (name, w, h, px))
    open(OUT_C, "w").write("\n".join(c))
    open(OUT_H, "w").write("\n".join(hdr) + "\n")


if __name__ == "__main__":
    main()
