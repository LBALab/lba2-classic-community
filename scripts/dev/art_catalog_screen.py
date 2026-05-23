#!/usr/bin/env python3
"""Extract every full-screen bitmap from SCREEN.HQR as a PNG using its paired palette.

SCREEN.HQR carries 39 pairs: even slots are 640x480 indexed bitmaps (307200 bytes),
the following odd slot is the 256x3 RGB palette (768 bytes).  Only a few have names
in COMMON.H (PCR_LOGO/BUMPER/MENU/ARDOISE/...); the rest are unnamed but loaded by
literal index from various game code paths.

This script is the inventory step of the widescreen art-catalog: it dumps every
bitmap to a PNG under docs/widescreen/catalog-dumps/ for visual review.  Output is
local-only (the directory's .gitignore covers it) since the bitmaps are copyrighted.

Usage:
  scripts/dev/art_catalog_screen.py <SCREEN.HQR> <OUTDIR>
"""
import argparse, os, sys

# Reuse decompression from the existing hqr_inspect.py.
_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
from hqr_inspect import entries, decompress_entry  # noqa: E402

from PIL import Image

# Known names from SOURCES/COMMON.H.  Index -> short tag used in filenames.
PCR_NAMES = {
    0:  "logo",        # PCR_LOGO       — Adeline logo
    2:  "bumper",      # PCR_BUMPER
    4:  "menu",        # PCR_MENU       — main-menu sky bg (the one in your reference screenshot)
    6:  "ardoise",     # PCR_ARDOISE    — slate (the in-game slate item Twinsen reads)
    8:  "pegout",      # PCR_PEGOUT     — Petit map "Egout" (sewer small)
    10: "aegout",      # PCR_AEGOUT     — Agrandi map "Egout" (sewer enlarged)
    12: "plabyrt",     # PCR_PLABYRT    — Petit map labyrinth
    14: "alabyrt",     # PCR_ALABYRT    — Agrandi map labyrinth
    16: "plune",       # PCR_PLUNE      — Petit map moon (Emerald Moon)
    18: "alune",       # PCR_ALUNE      — Agrandi map moon
    20: "hacienda",    # PCR_HACIENDA
    22: "vuphare",     # PCR_VUPHARE    — "Vue Phare" (lighthouse view, framed)
    24: "vuphare2",    # PCR_VUPHARE2   — lighthouse view, second frame (porthole vignette)
    60: "cdrom",       # PCR_CDROM      — CD-ROM splash
    72: "activision",  # PCR_ACTIVISION — distributor splash
    74: "ea",          # PCR_EA
    76: "virgin",      # PCR_VIRGIN
}

W, H = 640, 480

def render(bitmap, palette, out_path):
    """Write a single 640x480 PNG from indexed pixels + RGB palette."""
    assert len(bitmap) == W * H, f"bitmap is {len(bitmap)} bytes, expected {W*H}"
    assert len(palette) == 256 * 3, f"palette is {len(palette)} bytes, expected 768"
    img = Image.frombytes("P", (W, H), bitmap)
    img.putpalette(palette)
    img.convert("RGB").save(out_path)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("hqr")
    ap.add_argument("outdir")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)

    ents, raw = entries(args.hqr)

    pairs = []
    for i, off, size, csize, method in ents:
        if size is None or i % 2 == 1:
            continue
        # Expect entry i to be a 640x480 bitmap and entry i+1 its palette.
        if i + 1 >= len(ents):
            continue
        _, poff, psize, pcsize, pmethod = ents[i + 1]
        if psize is None:
            print(f"  [{i:>2}] missing palette pair; skipping", file=sys.stderr)
            continue
        if size != W * H:
            print(f"  [{i:>2}] unexpected bitmap size {size}; skipping", file=sys.stderr)
            continue
        if psize != 768:
            print(f"  [{i:>2}] unexpected palette size {psize}; skipping", file=sys.stderr)
            continue
        bitmap = decompress_entry(raw, off, size, csize, method)
        palette = decompress_entry(raw, poff, psize, pcsize, pmethod)
        # LBA2 palettes are stored as 6-bit values in the original DOS engine; the SDL
        # port left them as-is and shifts at SetVideoPalette time.  Normalise here so
        # PNGs look the same as in-engine.
        palette = bytes(min(c << 2 | c >> 4, 255) for c in palette) \
                  if max(palette) <= 0x3F else bytes(palette)
        name = PCR_NAMES.get(i, "unnamed")
        out = os.path.join(args.outdir, f"screen_{i:03d}_{name}.png")
        render(bitmap, palette, out)
        pairs.append((i, name, out))

    print(f"extracted {len(pairs)} bitmaps to {args.outdir}/")
    for i, name, p in pairs:
        print(f"  [{i:>2}] {name:<12} -> {os.path.basename(p)}")

if __name__ == "__main__":
    main()
