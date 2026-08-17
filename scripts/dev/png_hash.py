#!/usr/bin/env python3
"""Short content hash of a PNG's pixels.

Hashes decoded pixels rather than file bytes, so a re-encode with different
compression settings does not read as a change.

A note for anyone hashing UI captures: pass `--fixed-dt` to the engine when
taking them. The menus animate a plasma strip on the clock, so without a pinned
clock the same screen hashes differently every run, and any comparison built on
it reports regressions that are not there.

`--fixed-dt` makes a capture reproducible on one machine, but not across two.
The menu's plasma strip reaches a different state on Windows than on Linux and
stays there: repeated runs agree, tick counts do not move it, and the pixels are
the same 16-colour ramp either way, so it is the strip's animation state rather
than anything about how the menu is drawn. `--exclude` leaves that band out of
the hash, which is what lets one golden serve both platforms.

    png_hash.py shot.png
    png_hash.py shot.png --exclude 46,174,549,49     # x,y,w,h; repeatable

Excluded pixels are zeroed rather than skipped, so the hash still depends on
where the exclusion is: moving the band changes the digest instead of silently
comparing a different picture.

Python 3 standard library only, matching the other dev scripts.
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


def parse_rect(text):
    """`x,y,w,h` -> a 4-tuple of ints. Raises ValueError on anything else."""
    parts = text.split(",")
    if len(parts) != 4:
        raise ValueError("expected x,y,w,h, got %r" % text)
    rect = tuple(int(p) for p in parts)
    if any(v < 0 for v in rect) or rect[2] == 0 or rect[3] == 0:
        raise ValueError("rectangle must be non-negative and non-empty: %r" % text)
    return rect


def zero_rects(width, height, channels, pixels, rects):
    """Blank each rectangle so the hash ignores what is inside it.

    Out of bounds is an error rather than a clamp: a rectangle that no longer
    fits is a rectangle that has stopped describing what it was written for, and
    quietly hashing a clamped version of it is how a mask outlives its subject.
    """
    buf = bytearray(pixels)
    stride = width * channels
    for x, y, w, h in rects:
        if x + w > width or y + h > height:
            raise ValueError(
                "exclusion %d,%d,%d,%d falls outside the %dx%d image"
                % (x, y, w, h, width, height))
        for row in range(y, y + h):
            start = row * stride + x * channels
            buf[start:start + w * channels] = bytes(w * channels)
    return bytes(buf)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    rects = []
    i = 2
    try:
        while i < len(argv):
            if argv[i] != "--exclude" or i + 1 >= len(argv):
                raise ValueError("unexpected argument %r" % argv[i])
            rects.append(parse_rect(argv[i + 1]))
            i += 2
    except ValueError as exc:
        print("-")
        print("png_hash: %s" % exc, file=sys.stderr)
        return 2
    try:
        width, height, channels, pixels = decode(path)
        if rects:
            pixels = zero_rects(width, height, channels, pixels, rects)
    except Exception as exc:  # a missing or odd capture should not abort a sweep
        print("-")
        print("png_hash: %s" % exc, file=sys.stderr)
        return 1
    print(hashlib.sha256(pixels).hexdigest()[:12])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
