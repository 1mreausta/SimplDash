"""Väike TrueType lugeja: tähtede kontuurid (Barlow Condensed Bold) SVG/polügoonideks."""
import os
FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "main", "fonts", "BarlowCondensed-Bold.ttf")
# Minimal TrueType reader: glyph outlines -> SVG path (simple glyphs, quadratic curves)
import struct
data = open(FONT, 'rb').read()
num = struct.unpack('>H', data[4:6])[0]
tables = {}
for i in range(num):
    tag, cs, off, ln = struct.unpack('>4sIII', data[12+16*i:28+16*i]); tables[tag.decode()] = (off, ln)
def T(n): o, l = tables[n]; return data[o:o+l]
head = T('head'); upm = struct.unpack('>H', head[18:20])[0]; locfmt = struct.unpack('>h', head[50:52])[0]
maxp = T('maxp'); nglyphs = struct.unpack('>H', maxp[4:6])[0]
hhea = T('hhea'); nhm = struct.unpack('>H', hhea[34:36])[0]; hmtx = T('hmtx')
loca = T('loca')
offs = [struct.unpack('>H', loca[2*i:2*i+2])[0]*2 for i in range(nglyphs+1)] if locfmt == 0 else [struct.unpack('>I', loca[4*i:4*i+4])[0] for i in range(nglyphs+1)]
glyf = T('glyf')
# cmap format 4
cmap = T('cmap'); n = struct.unpack('>H', cmap[2:4])[0]; sub = None
for i in range(n):
    pid, eid, off = struct.unpack('>HHI', cmap[4+8*i:12+8*i])
    if struct.unpack('>H', cmap[off:off+2])[0] == 4: sub = cmap[off:]; break
segx2 = struct.unpack('>H', sub[6:8])[0]; sc = segx2//2
ends = struct.unpack('>%dH' % sc, sub[14:14+segx2]); starts = struct.unpack('>%dH' % sc, sub[16+segx2:16+2*segx2])
deltas = struct.unpack('>%dh' % sc, sub[16+2*segx2:16+3*segx2]); roff_pos = 16+3*segx2
roffs = struct.unpack('>%dH' % sc, sub[roff_pos:roff_pos+segx2])
def gid(ch):
    c = ord(ch)
    for i in range(sc):
        if starts[i] <= c <= ends[i]:
            if roffs[i] == 0: return (c + deltas[i]) & 0xFFFF
            p = roff_pos + 2*i + roffs[i] + 2*(c - starts[i]); g = struct.unpack('>H', sub[p:p+2])[0]
            return (g + deltas[i]) & 0xFFFF if g else 0
    return 0
def adv(g): g = min(g, nhm-1); return struct.unpack('>H', hmtx[4*g:4*g+2])[0]
def outline(g):
    d = glyf[offs[g]:offs[g+1]]; nc = struct.unpack('>h', d[0:2])[0]; assert nc >= 0
    xmin, ymin, xmax, ymax = struct.unpack('>4h', d[2:10])
    ec = struct.unpack('>%dH' % nc, d[10:10+2*nc]); npt = ec[-1]+1
    il = struct.unpack('>H', d[10+2*nc:12+2*nc])[0]; p = 12+2*nc+il
    flags = []
    while len(flags) < npt:
        f = d[p]; p += 1; flags.append(f)
        if f & 8: r = d[p]; p += 1; flags += [f]*r
    def coords(short, same):
        out = []; v = 0; nonlocal p
        for f in flags:
            if f & short: dv = d[p]; p += 1; v += dv if f & same else -dv
            elif not (f & same): v += struct.unpack('>h', d[p:p+2])[0]; p += 2
            out.append(v)
        return out
    xs = coords(2, 16); ys = coords(4, 32)
    contours = []; s = 0
    for e in ec:
        contours.append([(xs[i], ys[i], flags[i] & 1) for i in range(s, e+1)]); s = e+1
    return contours, (xmin, ymin, xmax, ymax)
def path(contours, tx, ty, k):
    P = lambda x, y: '%.2f %.2f' % (tx + x*k, ty - y*k)
    out = []
    for c in contours:
        n = len(c)
        # start at an on-curve point
        st = next((i for i in range(n) if c[i][2]), None)
        if st is None:
            a, b = c[0], c[1]; start = ((a[0]+b[0])/2, (a[1]+b[1])/2, 1); pts = c
        else:
            start = c[st]; pts = c[st+1:] + c[:st]
        out.append('M' + P(start[0], start[1])); prev_off = None
        for q in pts + [start]:
            if q[2]:
                out.append(('Q' + P(*prev_off) + ' ' + P(q[0], q[1])) if prev_off else ('L' + P(q[0], q[1]))); prev_off = None
            else:
                if prev_off:
                    mx, my = (prev_off[0]+q[0])/2, (prev_off[1]+q[1])/2
                    out.append('Q' + P(*prev_off) + ' ' + P(mx, my))
                prev_off = (q[0], q[1])
        out.append('Z')
    return ''.join(out)

CAP = outline(gid('H'))[1][3]
def text(t, x, baseline, cap_h, track=0.0):
    """Teksti tee: vasak tindiserv x, alusjoon baseline, suurtähe kõrgus cap_h. Tagastab (path, laius)."""
    k = cap_h / CAP
    gl = [gid(ch) for ch in t]; outs = [outline(g) for g in gl]
    pos = 0; placed = []
    for g, (cont, bb) in zip(gl, outs):
        placed.append((cont, pos)); pos += adv(g) + track / k
    ink_l = outs[0][1][0]; ink_r = placed[-1][1] + outs[-1][1][2]
    tx0 = x - ink_l * k
    return ''.join(path(c, tx0 + px * k, baseline, k) for c, px in placed), (ink_r - ink_l) * k
