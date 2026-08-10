#!/usr/bin/env python3
"""Short content hash of a PNG, with an optional excluded horizontal band.

Hashing the file bytes is not good enough for UI captures: the menus draw an
animated plasma strip whose pixels differ between two runs of the same build, so
a whole-image hash reports a change on every run and stops meaning anything.
This decodes the image and hashes the pixels outside a band given as fractions
of the height, so the stable part of the frame can be compared.

Python 3 standard library only, matching the other dev scripts.

    png_hash.py shot.png                 whole image
    png_hash.py shot.png 0.35 0.475      skip the plasma band
"""
import hashlib
import struct
import sys
import zlib

_CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


def decode(path):
    """Return (width, height, channels, raw pixel bytes) for a non-interlaced PNG."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG: %s" % path)
    width, height = struct.unpack(">II", data[16:24])
    bit_depth, colour_type, interlace = data[24], data[25], data[28]
    if bit_depth != 8 or interlace != 0:
        raise ValueError("only 8-bit non-interlaced PNGs are handled")

    idat = bytearray()
    pos = 8
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        if data[pos + 4:pos + 8] == b"IDAT":
            idat += data[pos + 8:pos + 8 + length]
        pos += 12 + length

    raw = zlib.decompress(bytes(idat))
    channels = _CHANNELS[colour_type]
    stride = width * channels

    out = bytearray()
    prev = bytearray(stride)
    pos = 0
    for _ in range(height):
        filt = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        # Undo the per-scanline filter (PNG spec section 9).
        for x in range(stride):
            left = line[x - channels] if x >= channels else 0
            up = prev[x]
            upleft = prev[x - channels] if x >= channels else 0
            if filt == 1:
                line[x] = (line[x] + left) & 0xFF
            elif filt == 2:
                line[x] = (line[x] + up) & 0xFF
            elif filt == 3:
                line[x] = (line[x] + (left + up) // 2) & 0xFF
            elif filt == 4:
                p = left + up - upleft
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upleft)
                pred = left if (pa <= pb and pa <= pc) else (up if pb <= pc else upleft)
                line[x] = (line[x] + pred) & 0xFF
        out += line
        prev = line
    return width, height, channels, bytes(out)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    skip_from = float(argv[2]) if len(argv) > 2 else None
    skip_to = float(argv[3]) if len(argv) > 3 else None

    try:
        width, height, channels, pixels = decode(path)
    except Exception as exc:  # a missing or odd capture should not abort a sweep
        print("-")
        print("png_hash: %s" % exc, file=sys.stderr)
        return 1

    lo = int(height * skip_from) if skip_from is not None else -1
    hi = int(height * skip_to) if skip_to is not None else -1

    digest = hashlib.sha256()
    stride = width * channels
    for y in range(height):
        if lo <= y < hi:
            continue
        digest.update(pixels[y * stride:(y + 1) * stride])
    print(digest.hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
