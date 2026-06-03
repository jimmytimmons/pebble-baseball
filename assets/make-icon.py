#!/usr/bin/env python3
"""Generate the Baseball Scores app icon at every appstore size.

Renders a baseball (white ball, navy ring, two diagonal red seams with stitch
ticks) on a transparent background, supersampled then area-averaged down to each
size. No third-party deps — just the standard library.

    python3 assets/make-icon.py        # writes icon-{144,80,50,25}.png next to this file
"""
import os, zlib, struct, math

S = 576                 # supersample canvas
k = S / 320.0           # scale factor so every dimension tracks the canvas size
CX = CY = S / 2.0
RO = 134 * k            # navy ring outer radius
RING = 15 * k
RI = RO - RING          # white ball radius
SIZES = [144, 80, 50, 25]

NAVY  = (22, 40, 74, 255)
WHITE = (252, 252, 250, 255)
GRAY  = (214, 217, 221, 255)     # subtle shadow
RED   = (208, 42, 46, 255)
CLEAR = (0, 0, 0, 0)

phi = math.radians(45)           # rotate the seam pattern to the diagonal orientation
cphi, sphi = math.cos(phi), math.sin(phi)
BASE, AMP = 66.0 * k, 20.0 * k   # seam offset from center: tips far, mid close


def seam_point(side, p):
    v = p * RI
    f = 1.0 - (v / RI) ** 2
    u = side * (BASE - AMP * f)
    return (CX + u * cphi - v * sphi, CY + u * sphi + v * cphi)


def render():
    px = [CLEAR] * (S * S)
    for y in range(S):
        for x in range(S):
            dx, dy = x - CX, y - CY
            d = math.hypot(dx, dy)
            if d > RO:
                continue
            if d >= RI:
                px[y * S + x] = NAVY
            else:
                px[y * S + x] = GRAY if (dx * 0.55 + dy * 0.85) > 46 * k else WHITE

    def stamp(p0, p1, half_w):
        x0, y0 = p0; x1, y1 = p1
        vx, vy = x1 - x0, y1 - y0
        L2 = vx * vx + vy * vy or 1.0
        for y in range(int(min(y0, y1) - half_w - 1), int(max(y0, y1) + half_w + 2)):
            if y < 0 or y >= S:
                continue
            for x in range(int(min(x0, x1) - half_w - 1), int(max(x0, x1) + half_w + 2)):
                if x < 0 or x >= S or math.hypot(x - CX, y - CY) > RI - 1:
                    continue
                t = ((x - x0) * vx + (y - y0) * vy) / L2
                t = 0.0 if t < 0 else 1.0 if t > 1 else t
                if math.hypot(x - (x0 + t * vx), y - (y0 + t * vy)) <= half_w:
                    px[y * S + x] = RED

    N = 11
    for side in (-1, 1):
        pts = [seam_point(side, -0.8 + 1.6 * j / (N - 1)) for j in range(N)]
        for j in range(N):
            x, y = pts[j]
            a = pts[max(0, j - 1)]; b = pts[min(N - 1, j + 1)]
            tx, ty = b[0] - a[0], b[1] - a[1]
            tl = math.hypot(tx, ty) or 1.0
            nx, ny = -ty / tl, tx / tl       # unit normal to the seam
            hh = 11 * k                       # stitch half-length
            stamp((x - nx * hh, y - ny * hh), (x + nx * hh, y + ny * hh), 2.8 * k)
    return px


def downscale(src, t):
    """Area-average src (S x S RGBA) down to t x t, premultiplying alpha."""
    out = [CLEAR] * (t * t)
    for ty in range(t):
        y0, y1 = ty * S / t, (ty + 1) * S / t
        for tx in range(t):
            x0, x1 = tx * S / t, (tx + 1) * S / t
            ar = ag = ab = aa = 0.0
            n = 0
            for sy in range(int(y0), int(math.ceil(y1))):
                for sx in range(int(x0), int(math.ceil(x1))):
                    r, g, b, a = src[sy * S + sx]
                    ar += r * a; ag += g * a; ab += b * a; aa += a
                    n += 1
            if aa > 0:
                out[ty * t + tx] = (round(ar / aa), round(ag / aa), round(ab / aa), round(aa / n))
    return out


def write_png(path, w, h, pix):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw += bytes(pix[y * w + x])
    def chunk(typ, data):
        return struct.pack(">I", len(data)) + typ + data + struct.pack(">I", zlib.crc32(typ + data) & 0xffffffff)
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))   # 8-bit RGBA
    out += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    out += chunk(b"IEND", b"")
    open(path, "wb").write(out)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    master = render()
    for sz in SIZES:
        write_png(os.path.join(here, "icon-%d.png" % sz), sz, sz, downscale(master, sz))
        print("wrote icon-%d.png" % sz)
