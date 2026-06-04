#!/usr/bin/env python3
"""Parse the Adeline 'XCF File Format' (.ACF) cinematic container used by
Time Commando (1996). Pre-dates LBA2's switch to Smacker, so SOURCES/PLAYACF.CPP
(libsmacker) does NOT decode it.

Container = a flat sequence of chunks, each:
    char tag[8]      # ASCII, space-padded ('FrameLen','Format  ','Palette ',
                     #   'Recouvre','Camera  ','KeyFrame','NulChunk','SoundBufD'...)
    u32  size        # little-endian, bytes of payload that follow
    u8   data[size]
Chunks are contiguous (next tag immediately follows the previous payload).
'NulChunk' is explicit padding used to align the following frame chunk.

Usage:
  acf_inspect.py <file.acf> [--max N] [--dump-palette OUT] [--dump TAG:N OUT]
"""
import struct, sys, argparse
from collections import Counter

def walk(data):
    """Yield (offset, tag, size, payload_slice_start)."""
    off = 0
    n = len(data)
    while off + 12 <= n:
        tag = data[off:off+8]
        # a valid tag is printable ASCII
        if not all(32 <= b < 127 for b in tag):
            break
        size = struct.unpack_from("<I", data, off+8)[0]
        payload = off + 12
        if payload + size > n:
            yield off, tag.decode("latin-1"), size, payload, False
            break
        yield off, tag.decode("latin-1"), size, payload, True
        off = payload + size

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--max", type=int, default=40, help="max chunks to print")
    ap.add_argument("--dump-palette", default=None)
    ap.add_argument("--summary", action="store_true", help="tag histogram only")
    a = ap.parse_args()
    data = open(a.path, "rb").read()

    chunks = list(walk(data))
    last_end = chunks[-1][3] + chunks[-1][2] if chunks else 0
    clean = bool(chunks) and chunks[-1][4] and last_end == len(data)

    # decode FrameLen header
    if chunks and chunks[0][1].strip() == "FrameLen":
        p = chunks[0][3]
        fcount, field2 = struct.unpack_from("<II", data, p)
        print(f"# FrameLen: frameCount={fcount}  field@+4=0x{field2:x} ({field2})")

    hist = Counter(t.strip() for _,t,_,_,_ in chunks)
    print(f"# {a.path}: {len(chunks)} chunks, walk {'CLEAN to EOF' if clean else 'STOPPED at %d/%d'%(last_end,len(data))}")
    print(f"# tag histogram: {dict(hist)}")
    if a.summary:
        return

    for i,(off,tag,size,payload,ok) in enumerate(chunks[:a.max]):
        head = data[payload:payload+12].hex(' ')
        flag = '' if ok else '  <TRUNCATED>'
        print(f"  @{off:>9} (0x{off:07x})  {tag!r:<11} size={size:>9}  data0={head}{flag}")
    if len(chunks) > a.max:
        print(f"  ... +{len(chunks)-a.max} more")

    if a.dump_palette:
        for off,tag,size,payload,ok in chunks:
            if tag.strip() == "Palette":
                open(a.dump_palette,"wb").write(data[payload:payload+size])
                print(f"# wrote {a.dump_palette} ({size} bytes)")
                break

if __name__ == "__main__":
    main()
