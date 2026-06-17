#!/usr/bin/env python3
"""Standalone visual gut-check for the LBA1 body decoder + skeleton (docs/LBA1_PORT_PLAN.md §6).

Renders decoded LBA1 body geometry to a grayscale PNG with a tiny built-in software rasteriser
(stdlib only). No engine build, no deps. Reuses the validated decoder in lba1_body_probe.py.

LBA1 bodies are animated: points are stored GROUP-LOCAL and must be assembled through the bone
hierarchy. The 38-byte group (from P_OBJET.ASM RotateGroupe / P_TRIGO.ASM RotList) is:
  +0 startPoint(x6)  +2 nbPoints  +4 pivotPoint(x6)  +6 parentGroup(x38, -1=root)
  +8 type  +10/12/14 angles  +18 nbNormal  +20 3x3 matrix (9 words, Q14 row-major)
Bind pose: assembled[i] = (M_group . rawPoint[i]) >> 14 + assembled[pivot]; root pivot = origin.

Usage:
  lba1_body_render.py [--lba1 LBA/BODY.HQR] [--entry N] [--outdir /tmp]
                      [--dumpgroups] [--raw] [--shift 14]
  (no --entry: render a few representative bodies)
"""
import argparse
import math
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lba1_body_probe as P  # noqa: E402


def write_png_gray(path, w, h, buf):
    def chunk(typ, data):
        body = typ + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += buf[y * w:(y + 1) * w]
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def fill_tri(buf, zbuf, W, H, p0, p1, p2, shade):
    def e(ax, ay, bx, by, cx, cy):
        return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax)
    minx = max(0, int(min(p0[0], p1[0], p2[0])))
    maxx = min(W - 1, int(max(p0[0], p1[0], p2[0])) + 1)
    miny = max(0, int(min(p0[1], p1[1], p2[1])))
    maxy = min(H - 1, int(max(p0[1], p1[1], p2[1])) + 1)
    area = e(p0[0], p0[1], p1[0], p1[1], p2[0], p2[1])
    if area == 0:
        return
    z0, z1, z2 = p0[2], p1[2], p2[2]
    for y in range(miny, maxy + 1):
        for x in range(minx, maxx + 1):
            px, py = x + 0.5, y + 0.5
            w0 = e(p1[0], p1[1], p2[0], p2[1], px, py)
            w1 = e(p2[0], p2[1], p0[0], p0[1], px, py)
            w2 = e(p0[0], p0[1], p1[0], p1[1], px, py)
            if not ((w0 >= 0 and w1 >= 0 and w2 >= 0) or (w0 <= 0 and w1 <= 0 and w2 <= 0)):
                continue
            z = (w0 * z0 + w1 * z1 + w2 * z2) / area
            o = y * W + x
            if z >= zbuf[o]:                # nearer (larger z toward viewer) wins
                zbuf[o] = z
                buf[o] = shade


def rot(p, yaw, pitch):
    x, y, z = p
    cy, sy = math.cos(yaw), math.sin(yaw)
    cx, sx = math.cos(pitch), math.sin(pitch)
    x1, z1 = x * cy + z * sy, -x * sy + z * cy
    y2, z2 = y * cx - z1 * sx, y * sx + z1 * cx
    return (x1, y2, z2)


def decode_group(g):
    return {"start": P.u16(g, 0) // 6, "nb": P.u16(g, 2),
            "pivot": P.u16(g, 4) // 6, "parent": P.s16(g, 6),
            "type": P.u16(g, 8), "angles": (P.s16(g, 10), P.s16(g, 12), P.s16(g, 14)),
            "nbnorm": P.u16(g, 18), "mat": [P.s16(g, 20 + 2 * k) for k in range(9)]}


def assemble(d):
    """Bind pose: walk the bone hierarchy (parents first) and offset each group's local points
    by its pivot on the parent. The body's per-group angles are all 0 (rest), so every group's
    rotation matrix is identity (RotMat of identity + zero angles); RotList then reduces to
    translation by assembled[pivot]. Per-frame limb rotations live in ANIM.HQR, not the body."""
    raw = d["points"]
    out = [None] * len(raw)
    for g in (decode_group(x) for x in d["groups"]):
        ox, oy, oz = (0, 0, 0) if g["parent"] == -1 else (out[g["pivot"]] or (0, 0, 0))
        for i in range(g["start"], min(g["start"] + g["nb"], len(raw))):
            x, y, z = raw[i]
            out[i] = (x + ox, y + oy, z + oz)
    return [out[i] if out[i] is not None else raw[i] for i in range(len(raw))]


def render(points, polys, path, W=360, H=460, yaw=0.5, pitch=-0.25):
    pts = [rot(q, yaw, pitch) for q in points]
    xs = [q[0] for q in pts]; ys = [q[1] for q in pts]
    minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
    scale = min((W - 40) / (maxx - minx or 1), (H - 40) / (maxy - miny or 1))

    def proj(q):
        return (20 + (q[0] - minx) * scale, H - 20 - (q[1] - miny) * scale, q[2])

    buf = bytearray([248]) * (W * H)
    zbuf = [-1e30] * (W * H)
    n = len(pts)
    tris = 0
    for pol in polys:
        v = [i for i in pol["verts"] if 0 <= i < n]
        for k in range(1, len(v) - 1):
            a, b, c = pts[v[0]], pts[v[k]], pts[v[k + 1]]
            ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
            vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
            nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
            nl = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            shade = int(55 + 175 * abs(nz) / nl)
            fill_tri(buf, zbuf, W, H, proj(a), proj(b), proj(c), shade)
            tris += 1
    write_png_gray(path, W, H, buf)
    return tris


def main():
    here = os.path.abspath(__file__)
    hacking = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(here))))
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lba1", default=os.path.join(hacking, "LBA", "BODY.HQR"))
    ap.add_argument("--entry", type=int, default=None)
    ap.add_argument("--outdir", default="/tmp")
    ap.add_argument("--raw", action="store_true", help="render group-local points (no skeleton)")
    ap.add_argument("--shift", type=int, default=14, help="matrix fixed-point shift (Q14)")
    ap.add_argument("--dumpgroups", action="store_true")
    a = ap.parse_args()
    if not os.path.exists(a.lba1):
        print(f"MISSING {a.lba1}"); return 2

    entries = [a.entry] if a.entry is not None else [1, 51, 80, 120]
    for entry in entries:
        b, _, _ = P.hqr_entry(a.lba1, entry)
        d = P.decode_lba1_body(b)
        if a.dumpgroups:
            print(f"body {entry}: {len(d['groups'])} groups")
            for gi, gb in enumerate(d["groups"]):
                g = decode_group(gb)
                print(f"  g{gi:2}: start={g['start']:4} nb={g['nb']:3} pivot={g['pivot']:4} "
                      f"parent={g['parent']:4} type={g['type']} ang={g['angles']} "
                      f"matdiag=({g['mat'][0]},{g['mat'][4]},{g['mat'][8]})")
            continue
        pts = d["points"] if a.raw else assemble(d)
        out = os.path.join(a.outdir, f"lba1_body_{entry}{'_raw' if a.raw else ''}.png")
        ntri = render(pts, d["polys"], out)
        ys = [p[1] for p in pts]
        print(f"  body {entry}: {len(d['groups'])} groups, {len(pts)} pts, {len(d['polys'])} polys "
              f"-> {ntri} tris; assembled Y span [{min(ys)},{max(ys)}] -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
