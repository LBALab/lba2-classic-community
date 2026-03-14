#!/usr/bin/env python3
"""Parse and display the header info from a .lba2snap file."""
import struct, sys

path = sys.argv[1] if len(sys.argv) > 1 else "tests/SNAPSHOT/fixtures/snapshot_0000.lba2snap"
with open(path, "rb") as f:
    magic = f.read(8)
    version, nsec = struct.unpack("<II", f.read(8))
    print(f"Magic: {magic}  Version: {version}  Sections: {nsec}")
    # SHARED_STATE section header
    st, ss = struct.unpack("<II", f.read(8))
    print(f"Section type: {st:#010x}  size: {ss}")
    cx, cy, cz = struct.unpack("<iii", f.read(12))
    ca, cb, cg = struct.unpack("<iii", f.read(12))
    xr, yr, zr = struct.unpack("<iii", f.read(12))
    zrc, nc = struct.unpack("<ii", f.read(8))
    tp = struct.unpack("<i", f.read(4))[0]
    xc, yc = struct.unpack("<ii", f.read(8))
    fx, fy = struct.unpack("<ff", f.read(8))
    lx, ly = struct.unpack("<ii", f.read(8))
    print(f"Camera pos: ({cx}, {cy}, {cz})")
    print(f"Camera rot: ({ca}, {cb}, {cg})")
    print(f"Camera rotated: ({xr}, {yr}, {zr})")
    print(f"ZrClip: {zrc}  NearClip: {nc}")
    print(f"TypeProj: {tp}  Centre: ({xc}, {yc})")
    print(f"FRatio: ({fx:.4f}, {fy:.4f})  LFactor: ({lx}, {ly})")
