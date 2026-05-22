#!/usr/bin/env python3
"""Inspect an Adeline HQR archive: list entries (index, offset, sizes, compression).

HQR format (see LIB386/SYSTEM/HQFILE.CPP, HQR.H):
  - File begins with a U32 offset table. The first U32 is the byte offset of entry 0,
    so the table holds first_offset/4 slots. Each slot is a byte offset to that entry's
    header (0 = empty/missing entry).
  - Each entry: COMPRESSED_HEADER { u32 SizeFile; u32 CompressedSizeFile; s16 CompressMethod }
    followed by the data. CompressMethod: 0 stored, 1 LZSS, 2 LZMIT.

Usage:
  hqr_inspect.py <file.hqr> [--entry N] [--limit N] [--dump OUT]
"""
import struct, sys, argparse

def expand_lz(src, decomp_size, min_bloc):
    """Port of ExpandLZ (LIB386/SYSTEM/LZ.CPP). min_bloc = CompressMethod + 1."""
    dst = bytearray(decomp_size)
    s = d = 0
    while d < decomp_size:
        flag = src[s]; s += 1
        for _ in range(8):
            if d >= decomp_size:
                break
            if flag & 1:
                dst[d] = src[s]; s += 1; d += 1
            else:
                length = (src[s] & 0x0F) + min_bloc
                offset = (src[s + 1] << 4) | (src[s] >> 4)
                s += 2
                for _ in range(length):
                    if d >= decomp_size:
                        break
                    dst[d] = dst[d - offset - 1]; d += 1
            flag >>= 1
    return bytes(dst)

def decompress_entry(data, off, size, csize, method):
    if method == 0:
        return data[off + 10 : off + 10 + size]
    return expand_lz(data[off + 10 : off + 10 + csize], size, method + 1)

def entries(path):
    with open(path, "rb") as f:
        data = f.read()
    (first,) = struct.unpack_from("<I", data, 0)
    n = first // 4
    out = []
    for i in range(n):
        (off,) = struct.unpack_from("<I", data, i * 4)
        if off == 0 or off + 10 > len(data):
            out.append((i, off, None, None, None))
            continue
        size, csize, method = struct.unpack_from("<IIh", data, off)
        out.append((i, off, size, csize, method))
    return out, data

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--entry", type=int, default=None)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--dump", default=None, help="decompress --entry to this file")
    a = ap.parse_args()
    ents, data = entries(a.path)
    meth = {0: "stored", 1: "LZSS", 2: "LZMIT"}
    present = [e for e in ents if e[2] is not None]
    print(f"{a.path}: {len(ents)} slots, {len(present)} present")
    rows = ents if a.entry is None else [ents[a.entry]]
    if a.limit and a.entry is None:
        rows = rows[: a.limit]
    for i, off, size, csize, m in rows:
        if size is None:
            print(f"  [{i:4}] empty")
        else:
            ratio = f"{csize*100//size if size else 0}%"
            print(f"  [{i:4}] off={off:>9}  size={size:>7}  comp={csize:>7} ({ratio})  {meth.get(m, m)}")
    if a.entry is not None and present and ents[a.entry][2] is not None:
        i, off, size, csize, m = ents[a.entry]
        out = decompress_entry(data, off, size, csize, m)
        print(f"  decompressed {len(out)} bytes (expected {size})")
        print(f"  first bytes: {out[:48].hex(' ')}")
        if a.dump:
            with open(a.dump, "wb") as f:
                f.write(out)
            print(f"  wrote {a.dump}")

if __name__ == "__main__":
    main()
