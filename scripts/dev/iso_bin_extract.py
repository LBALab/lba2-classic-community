#!/usr/bin/env python3
"""Extract a single file from a raw Mode-1 BIN ISO9660 image.

Companion to `iso_bin_reader.py` — same on-disk format assumptions
(2352-byte sectors with 16-byte sync+header + 2048 bytes user data +
288 bytes ECC/EDC). Once `iso_bin_reader.py` has shown the file path
inside the image, use this to pull a single file out for inspection.

Used originally to extract `SETUP.MID` from GOG's `LBA2.GOG` to confirm
it was a real Standard MIDI File (it is — see issue #119 and
docs/AUDIO.md). Useful for any "what's in this BIN" debugging.

Usage:
    python3 scripts/dev/iso_bin_extract.py <image.bin> <iso-path> <output>

Example:
    python3 scripts/dev/iso_bin_extract.py LBA2.GOG /LBA2/SETUP.MID /tmp/SETUP.MID

Path lookup is case-insensitive (matches ISO9660 conventions on most
DOS-era discs).
"""
import sys, struct

BIN = sys.argv[1]
TARGET = sys.argv[2]
OUTPATH = sys.argv[3]

SECTOR = 2352
DATA_OFFSET = 16
DATA_SIZE = 2048

def read_sector(f, lba):
    f.seek(lba * SECTOR + DATA_OFFSET)
    return f.read(DATA_SIZE)

def parse_dir(buf):
    out = []
    i = 0
    while i < len(buf):
        rec_len = buf[i] if i < len(buf) else 0
        if rec_len == 0:
            i = (i // DATA_SIZE + 1) * DATA_SIZE
            if i >= len(buf):
                break
            continue
        lba = struct.unpack_from("<I", buf, i+2)[0]
        size = struct.unpack_from("<I", buf, i+10)[0]
        flags = buf[i+25]
        nl = buf[i+32]
        name = buf[i+33:i+33+nl].decode("latin-1", errors="replace").split(";")[0]
        if name not in ("\x00", "\x01"):
            out.append((name, lba, size, bool(flags & 0x02)))
        i += rec_len
    return out

def read_data(f, lba, size):
    chunks = []
    rem = size
    s = lba
    while rem > 0:
        d = read_sector(f, s)
        chunks.append(d[:min(DATA_SIZE, rem)])
        rem -= DATA_SIZE
        s += 1
    return b"".join(chunks)

def find(f, lba, size, parts):
    raw = read_data(f, lba, size)
    for name, clba, csize, isdir in parse_dir(raw):
        if name.upper() == parts[0].upper():
            if len(parts) == 1:
                return (clba, csize)
            if isdir:
                return find(f, clba, csize, parts[1:])
    return None

with open(BIN, "rb") as f:
    pvd = read_sector(f, 16)
    rl = struct.unpack_from("<I", pvd, 156+2)[0]
    rs = struct.unpack_from("<I", pvd, 156+10)[0]
    parts = TARGET.strip("/").split("/")
    res = find(f, rl, rs, parts)
    if not res:
        print("not found")
        sys.exit(1)
    lba, size = res
    data = read_data(f, lba, size)
    with open(OUTPATH, "wb") as o:
        o.write(data)
    print(f"wrote {size} bytes")
