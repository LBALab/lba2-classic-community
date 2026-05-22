#!/usr/bin/env python3
"""Decode Adeline POF wireframe shapes (the RESS_POF=46 blob in ress.hqr).

A POF is a small 2D point-and-line wireframe (spark/star/debris outline) drawn scaled and
rotated as an effect, via PofDisplay3DExt (SOURCES/POF.CPP). IMPACT's THROW_POF/FLOW_POF
reference one by index; InitExtraPof spawns a flying extra that renders it. Same container as
IMPACT: a U32 offset table (count = first/4), each slot a byte offset to a POF.

Per-POF layout (POF.CPP): u8 coul; u8 nb_points; (s16 x, s16 y) * nb_points;
                          u8 nb_lines; (u8 p0, u8 p1) * nb_lines.

Usage: pof_dump.py <pof.bin>   (decompress RESS_POF=46 via hqr_inspect.py --entry 46 --dump)
"""
import struct, sys

def parse(d, off):
    p = off
    coul = d[p]; p += 1
    npt = d[p]; p += 1
    pts = []
    for _ in range(npt):
        x, y = struct.unpack_from("<hh", d, p); p += 4
        pts.append((x, y))
    nln = d[p]; p += 1
    lines = []
    for _ in range(nln):
        a, b = d[p], d[p+1]; p += 2
        lines.append((a, b))
    return (coul, pts, lines), p

def assemble(pof):
    coul, pts, lines = pof
    out = bytearray([coul, len(pts)])
    for x, y in pts:
        out += struct.pack("<hh", x, y)
    out.append(len(lines))
    for a, b in lines:
        out += bytes([a, b])
    return bytes(out)

def main():
    d = open(sys.argv[1], "rb").read()
    n = struct.unpack_from("<I", d, 0)[0] // 4
    offs = [struct.unpack_from("<I", d, i*4)[0] for i in range(n)]
    print(f"{len(d)} bytes, {n} POF slots")
    streams = []
    for i in range(n):
        s = offs[i]; e = offs[i+1] if i+1 < n else len(d)
        if s >= len(d) or s == e:
            print(f"pof {i}: (empty)"); streams.append(b""); continue
        pof, end = parse(d, s)
        coul, pts, lines = pof
        print(f"pof {i:3}: coul={coul:>3} points={len(pts):>3} lines={len(lines):>3} "
              f"({end-s} bytes, slot {e-s})")
        streams.append(assemble(pof))
    # round-trip (rebuild offset table + bodies)
    cur = n*4; offs2 = []
    for st in streams: offs2.append(cur); cur += len(st)
    rebuilt = b"".join(struct.pack("<I", o) for o in offs2) + b"".join(streams)
    print(f"\nround-trip: {'BYTE-IDENTICAL' if rebuilt == d else 'DIFFERS'}")

if __name__ == "__main__":
    main()
