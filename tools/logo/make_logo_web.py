#!/usr/bin/env python3
"""
Teeb docs/logo.svg-st veebilehe logo docs/logo_web.svg: ilma taustata ja ilma tühja ääreta
(viewBox = täpselt teksti ja tulede ümber), et logo vasak serv oleks lehe sisuga samal joonel.

    python3 tools/logo/make_logo_web.py
"""
import math
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "..", "docs", "logo.svg")
DST = os.path.join(HERE, "..", "..", "docs", "logo_web.svg")

svg = open(SRC, encoding="utf-8").read()

# tekstirühma kalle: translate(160 58) skewX(-12) translate(-160 -58)  →  x' = x + tan(-12°)·(y − 58)
K = math.tan(math.radians(-12))
pts = []


def path_points(d):
    """M/L/Q/Z absoluutsed käsud → punktid (kõverad tihedalt läbi käidud)."""
    out, cur = [], (0.0, 0.0)
    for cmd, args in re.findall(r"([MLQZ])([^MLQZ]*)", d):
        n = [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", args)]
        if cmd in "ML":
            for i in range(0, len(n), 2):
                cur = (n[i], n[i + 1])
                out.append(cur)
        elif cmd == "Q":
            for i in range(0, len(n), 4):
                (cx, cy), (ex, ey) = (n[i], n[i + 1]), (n[i + 2], n[i + 3])
                for k in range(1, 9):
                    t = k / 8
                    x = (1 - t) ** 2 * cur[0] + 2 * (1 - t) * t * cx + t * t * ex
                    y = (1 - t) ** 2 * cur[1] + 2 * (1 - t) * t * cy + t * t * ey
                    out.append((x, y))
                cur = (ex, ey)
    return out


g = re.search(r"<g transform=[^>]*>(.*?)</g>", svg, re.S).group(1)
for d in re.findall(r' d="([^"]+)"', g):
    pts += [(x + K * (y - 58), y) for x, y in path_points(d)]
# tulede rida (ristkülikud väljaspool rühma)
for x, y, w, h in re.findall(r'<rect x="([\d.]+)" y="([\d.]+)" width="([\d.]+)" height="([\d.]+)"', svg):
    x, y, w, h = map(float, (x, y, w, h))
    pts += [(x, y), (x + w, y + h)]

x0 = min(p[0] for p in pts)
x1 = max(p[0] for p in pts)
y0 = min(p[1] for p in pts)
y1 = max(p[1] for p in pts)
vb = "%.2f %.2f %.2f %.2f" % (x0, y0, x1 - x0, y1 - y0)

out = re.sub(r'viewBox="[^"]*"', 'viewBox="%s"' % vb, svg, count=1)
out = re.sub(r'\s(width|height)="\d+"', "", out, count=2)
out = re.sub(r'<defs><linearGradient id="a_bg".*?</defs>', "", out, count=1, flags=re.S)
out = re.sub(r'<rect width="320" height="96"[^>]*/>', "", out, count=1)
open(DST, "w", encoding="utf-8").write(out)
print("docs/logo_web.svg: viewBox %s" % vb)
