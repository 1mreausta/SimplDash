#!/usr/bin/env python3
"""Teeb main/web_page.c failist page.html (+ logo docs/logo_web.svg) (veebilehe HTML C-stringina).  python3 tools/webpage/gen.py"""
import os
here = os.path.dirname(os.path.abspath(__file__))
html = open(os.path.join(here, "page.html"), encoding="utf-8").read()
# logo ilma tausta ja ääreta (tools/logo/make_logo_web.py → docs/logo_web.svg)
logo = open(os.path.join(here, "..", "..", "docs", "logo_web.svg"), encoding="utf-8").read().strip()
html = html.replace("<!--LOGO-->", logo)
out = ['/* Genereeritud: tools/webpage/gen.py (allikas tools/webpage/page.html) – ära muuda käsitsi. */',
       '#include "web_api.h"', '', 'const char web_page_html[] =']
for line in html.split("\n"):
    esc = line.replace("\\", "\\\\").replace('"', '\\"')
    out.append('    "%s\\n"' % esc)
out[-1] += ";"
open(os.path.join(here, "..", "..", "main", "web_page.c"), "w", encoding="utf-8").write("\n".join(out) + "\n")
print("main/web_page.c: %d baiti HTML" % len(html.encode()))
