#!/usr/bin/env python3
"""Render SCREEN.HQR bitmaps with widescreen treatments applied, as PNG previews.

Implements the four runtime treatments specified in docs/widescreen/art-strategy.md:

  letterbox     centre the 640x480 bitmap, fill margins with black (or fill_index)
  palette-fill  same, but fill margins with the dominant edge palette index
                (auto-detected, or override with --fill-index)
  edge-clone    centre the bitmap, extend each row's edge colour into the margins
                (per-row column stretch — preserves vertical structure)
  mirror-tile   centre the bitmap, mirror the leftmost/rightmost margin-width
                strip into the margins

Output is a target_width x 480 RGB PNG. Default target width is 853 (16:9 at
480 height, matching docs/WIDESCREEN.md target).

The dumps live under docs/widescreen/catalog-dumps/ alongside the catalog
extractor's output — same .gitignore covers them (copyright concern).

Usage:
  # Single entry, single treatment
  scripts/dev/art_treatment_preview.py SCREEN.HQR --entry 4 \\
      --treatment edge-clone --out preview/menu_edge_clone.png

  # All entries with their strategy-assigned treatments
  scripts/dev/art_treatment_preview.py SCREEN.HQR --all --outdir preview/

  # Side-by-side comparison of all four treatments for one entry
  scripts/dev/art_treatment_preview.py SCREEN.HQR --entry 4 --compare \\
      --out preview/menu_compare.png
"""
import argparse, os, sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
from hqr_inspect import entries, decompress_entry  # noqa: E402

from PIL import Image

W, H = 640, 480
DEFAULT_TARGET_W = 853  # 16:9 at 480 height (480 * 16/9 = 853.33)

# Treatment assignments from docs/widescreen/art-strategy.md.
# Each entry index -> (treatment, optional hint).  Hint is a short note like
# "(white)" or "(black)" describing the expected fill colour; auto-detection
# should land on the same index for well-formed bitmaps.
TRIAGE = {
    0:  ("palette-fill", "white"),    # PCR_LOGO
    2:  ("edge-clone",   "stars"),    # PCR_BUMPER
    4:  ("palette-fill", "dark sky"), # PCR_MENU — cheap-test revised from edge-clone (stripes)
    6:  ("palette-fill", "dark wood"),# PCR_ARDOISE
    8:  ("edge-clone",   "wood"),     # PCR_PEGOUT
    10: ("palette-fill", "black"),    # PCR_AEGOUT
    12: ("edge-clone",   "wood"),     # PCR_PLABYRT
    14: ("palette-fill", "black"),    # PCR_ALABYRT
    16: ("edge-clone",   "wood"),     # PCR_PLUNE
    18: ("palette-fill", "black"),    # PCR_ALUNE
    20: ("letterbox",    None),       # PCR_HACIENDA — baked vignette
    22: ("letterbox",    None),       # PCR_VUPHARE
    24: ("letterbox",    None),       # PCR_VUPHARE2 — porthole
    26: ("palette-fill", "black"),    # planet + comet
    28: ("letterbox",    None),       # Funfrock lab
    30: ("letterbox",    None),       # picnic
    32: ("letterbox",    None),       # Emerald Moon porthole
    34: ("letterbox",    None),       # cell with orbs
    36: ("palette-fill", "black"),    # celestial diagram
    38: ("mirror-tile",  "wallpaper"),# Mona Lisa wallpaper
    40: ("edge-clone",   "paper"),    # volcano paper map
    42: ("palette-fill", "black"),    # volcano slate map
    44: ("letterbox",    None),       # album + guard
    46: ("edge-clone",   "smoke"),    # bust on plinth
    48: ("letterbox",    None),       # musicians
    50: ("letterbox",    None),       # prison cell
    52: ("letterbox",    None),       # dragon-fire
    54: ("letterbox",    None),       # red-sky forum
    56: ("palette-fill", "black"),    # spaceship + planet — cheap-test revised from edge-clone (nebula stripes)
    58: ("palette-fill", "black"),    # explosion celebration
    60: ("palette-fill", "black"),    # PCR_CDROM
    62: ("palette-fill", "black"),    # Sendell baby orb
    64: ("edge-clone",   "paper"),    # paper house map
    66: ("palette-fill", "black"),    # slate house map
    68: ("letterbox",    None),       # Twinsen + medallion
    70: ("letterbox",    None),       # Sendell-form Twinsen
    72: ("palette-fill", "white"),    # PCR_ACTIVISION
    74: ("palette-fill", "black"),    # PCR_EA
    76: ("palette-fill", "white"),    # PCR_VIRGIN
}

# ---------------------------------------------------------------------------
# HQR loading
# ---------------------------------------------------------------------------

def load_bitmap_pair(hqr_path, entry):
    """Return (indexed_bitmap_bytes, rgb_palette_bytes) for the bitmap at `entry`.

    Palette is at `entry+1`.  6-bit DOS-VGA palette values are normalised to
    8-bit by replicating the high bits low (`c<<2 | c>>4`)."""
    ents, raw = entries(hqr_path)
    if entry + 1 >= len(ents):
        raise ValueError(f"entry {entry} or its palette out of range")
    _, off,  size,  csize,  method  = ents[entry]
    _, poff, psize, pcsize, pmethod = ents[entry + 1]
    if size  != W * H:    raise ValueError(f"entry {entry}: expected {W*H} bytes, got {size}")
    if psize != 256 * 3:  raise ValueError(f"entry {entry+1}: expected 768-byte palette, got {psize}")
    bitmap  = decompress_entry(raw, off,  size,  csize,  method)
    palette = decompress_entry(raw, poff, psize, pcsize, pmethod)
    palette = bytes(min(c << 2 | c >> 4, 255) for c in palette) \
              if max(palette) <= 0x3F else bytes(palette)
    return bitmap, palette

def to_rgb_image(bitmap, palette):
    """640x480 indexed bytes + 768-byte RGB palette -> PIL RGB image."""
    img = Image.frombytes("P", (W, H), bitmap)
    img.putpalette(palette)
    return img.convert("RGB")

def palette_color(palette, index):
    """Look up palette index -> (r, g, b) tuple."""
    i = (index & 0xFF) * 3
    return (palette[i], palette[i + 1], palette[i + 2])

# ---------------------------------------------------------------------------
# Edge analysis
# ---------------------------------------------------------------------------

def dominant_edge_index(bitmap):
    """Return the most common palette index across the leftmost+rightmost columns.

    Used by palette-fill when no explicit --fill-index is given."""
    hist = [0] * 256
    for y in range(H):
        hist[bitmap[y * W + 0]] += 1
        hist[bitmap[y * W + (W - 1)]] += 1
    return max(range(256), key=lambda i: hist[i])

# ---------------------------------------------------------------------------
# Treatments
# ---------------------------------------------------------------------------

def margins(target_w):
    """Split the wide canvas into (left_margin, right_margin) around a 640-wide centre."""
    if target_w < W:
        raise ValueError(f"target width {target_w} smaller than bitmap width {W}")
    extra = target_w - W
    left = extra // 2
    return left, extra - left

def apply_letterbox(rgb, target_w, fill_color=(0, 0, 0)):
    """Centre + flat fill (default black)."""
    canvas = Image.new("RGB", (target_w, H), fill_color)
    lm, _ = margins(target_w)
    canvas.paste(rgb, (lm, 0))
    return canvas

def apply_palette_fill(rgb, target_w, bitmap, palette, fill_index=None):
    """Centre + flat fill from edge palette (auto-detected unless given)."""
    if fill_index is None:
        fill_index = dominant_edge_index(bitmap)
    return apply_letterbox(rgb, target_w, palette_color(palette, fill_index))

def apply_edge_clone(rgb, target_w):
    """Centre + per-row column stretch.  Each row in the left margin takes the
    colour of that row's leftmost source pixel; right margin mirrors the rule."""
    lm, rm = margins(target_w)
    canvas = Image.new("RGB", (target_w, H), (0, 0, 0))
    canvas.paste(rgb, (lm, 0))
    if lm > 0:
        left_col = rgb.crop((0, 0, 1, H)).resize((lm, H), Image.NEAREST)
        canvas.paste(left_col, (0, 0))
    if rm > 0:
        right_col = rgb.crop((W - 1, 0, W, H)).resize((rm, H), Image.NEAREST)
        canvas.paste(right_col, (lm + W, 0))
    return canvas

def apply_mirror_tile(rgb, target_w):
    """Centre + mirror the edge strip outward (strip width = margin width)."""
    lm, rm = margins(target_w)
    canvas = Image.new("RGB", (target_w, H), (0, 0, 0))
    canvas.paste(rgb, (lm, 0))
    if lm > 0:
        strip = rgb.crop((0, 0, lm, H)).transpose(Image.FLIP_LEFT_RIGHT)
        canvas.paste(strip, (0, 0))
    if rm > 0:
        strip = rgb.crop((W - rm, 0, W, H)).transpose(Image.FLIP_LEFT_RIGHT)
        canvas.paste(strip, (lm + W, 0))
    return canvas

TREATMENTS = {
    "letterbox":    lambda rgb, w, b, p, fi: apply_letterbox(rgb, w),
    "palette-fill": lambda rgb, w, b, p, fi: apply_palette_fill(rgb, w, b, p, fi),
    "edge-clone":   lambda rgb, w, b, p, fi: apply_edge_clone(rgb, w),
    "mirror-tile":  lambda rgb, w, b, p, fi: apply_mirror_tile(rgb, w),
}

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def render_one(hqr_path, entry, treatment, target_w, fill_index=None):
    bitmap, palette = load_bitmap_pair(hqr_path, entry)
    rgb = to_rgb_image(bitmap, palette)
    fn = TREATMENTS.get(treatment)
    if fn is None:
        raise ValueError(f"unknown treatment {treatment!r}; choose from {list(TREATMENTS)}")
    return fn(rgb, target_w, bitmap, palette, fill_index)

def render_compare(hqr_path, entry, target_w):
    """Four treatments side by side, stacked vertically, with labels."""
    from PIL import ImageDraw
    bitmap, palette = load_bitmap_pair(hqr_path, entry)
    rgb = to_rgb_image(bitmap, palette)
    label_h = 24
    rows = []
    for name in ("letterbox", "palette-fill", "edge-clone", "mirror-tile"):
        img = TREATMENTS[name](rgb, target_w, bitmap, palette, None)
        labelled = Image.new("RGB", (target_w, H + label_h), (32, 32, 32))
        labelled.paste(img, (0, label_h))
        draw = ImageDraw.Draw(labelled)
        draw.text((8, 4), name, fill=(255, 255, 255))
        rows.append(labelled)
    combined = Image.new("RGB", (target_w, (H + label_h) * 4), (0, 0, 0))
    for i, row in enumerate(rows):
        combined.paste(row, (0, i * (H + label_h)))
    return combined

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("hqr")
    ap.add_argument("--entry", type=int, help="single entry index (even, paired with palette at +1)")
    ap.add_argument("--treatment", choices=list(TREATMENTS),
                    help="treatment for --entry; default uses the triage table")
    ap.add_argument("--width", type=int, default=DEFAULT_TARGET_W,
                    help=f"target canvas width (default {DEFAULT_TARGET_W} = 16:9 @ 480h)")
    ap.add_argument("--fill-index", type=int, default=None,
                    help="palette index for palette-fill (default: auto-detect from edges)")
    ap.add_argument("--all", action="store_true",
                    help="render every triaged entry to --outdir with its strategy treatment")
    ap.add_argument("--compare", action="store_true",
                    help="stack all four treatments for one --entry into one PNG")
    ap.add_argument("--out", help="output PNG path (single-entry mode)")
    ap.add_argument("--outdir", help="output directory (--all mode)")
    args = ap.parse_args()

    if args.all:
        if not args.outdir:
            ap.error("--all requires --outdir")
        os.makedirs(args.outdir, exist_ok=True)
        for entry in sorted(TRIAGE):
            treatment, hint = TRIAGE[entry]
            img = render_one(args.hqr, entry, treatment, args.width)
            tag = treatment.replace("-", "_")
            out = os.path.join(args.outdir, f"screen_{entry:03d}_{tag}.png")
            img.save(out)
            note = f" ({hint})" if hint else ""
            print(f"  [{entry:>2}] {treatment:<13}{note:<14} -> {os.path.basename(out)}")
        return

    if args.entry is None:
        ap.error("provide --entry N (or --all)")

    if args.compare:
        if not args.out:
            ap.error("--compare requires --out")
        render_compare(args.hqr, args.entry, args.width).save(args.out)
        print(f"compare grid -> {args.out}")
        return

    treatment = args.treatment
    if treatment is None:
        if args.entry in TRIAGE:
            treatment, _ = TRIAGE[args.entry]
        else:
            ap.error("entry not in triage table; pass --treatment explicitly")
    if not args.out:
        ap.error("--out required for single-entry render")
    render_one(args.hqr, args.entry, treatment, args.width, args.fill_index).save(args.out)
    print(f"entry {args.entry} ({treatment}) -> {args.out}")

if __name__ == "__main__":
    main()
