#!/usr/bin/env python3
"""Render SCREEN.HQR bitmaps with widescreen treatments applied, as PNG previews.

Implements the runtime treatments specified in docs/widescreen/art-strategy.md:

  letterbox     centre the 640x480 bitmap, fill margins with black (or fill_index)
  palette-fill  same, but fill margins with the dominant edge palette index
                (auto-detected, or override with --fill-index)
  edge-clone    centre the bitmap, extend each row's edge colour into the margins
                (per-row column stretch — preserves vertical structure)
  mirror-tile   centre the bitmap, mirror the leftmost/rightmost margin-width
                strip into the margins
  stretch       resample the bitmap horizontally to target width (bilinear).
                Distorts but no margins; trades fidelity for fill.
  gradient      ignore the bitmap, fill the canvas with a procedural vertical
                blue gradient. Replacement treatment for PCR_MENU when no
                derivative of the original art reads well.

Output is a target_width x 480 RGB PNG. Default target width is 853 (16:9 at
480 height, matching docs/WIDESCREEN.md target).

The dumps live under docs/widescreen/catalog-dumps/ alongside the catalog
extractor's output — same .gitignore covers them (copyright concern).

Usage:
  # Single entry, single treatment
  scripts/dev/art_treatment_preview.py SCREEN.HQR --entry 4 \\
      --treatment stretch --out preview/menu_stretch.png

  # All entries with their strategy-assigned treatments
  scripts/dev/art_treatment_preview.py SCREEN.HQR --all --outdir preview/

  # Side-by-side comparison of every treatment for one entry
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
# "(white)" describing the expected fill behaviour.
#
# Triage was simplified after a second round of visual review (recorded in
# cheap-test-findings.md): letterbox is the default, palette-fill is kept
# only for the pure-white-background distributor/dev logos where the margin
# fill is genuinely invisible, and PCR_MENU is an open question — the
# letterbox option here is the fallback; `stretch` and `gradient` are the
# candidates being weighed.
TRIAGE = {
    0:  ("palette-fill", "white"),    # PCR_LOGO — white bg, invisible fill
    2:  ("letterbox",    None),       # PCR_BUMPER
    4:  ("letterbox",    None),       # PCR_MENU — see notes; stretch/gradient also viable
    6:  ("letterbox",    None),       # PCR_ARDOISE
    8:  ("letterbox",    None),       # PCR_PEGOUT
    10: ("letterbox",    None),       # PCR_AEGOUT
    12: ("letterbox",    None),       # PCR_PLABYRT
    14: ("letterbox",    None),       # PCR_ALABYRT
    16: ("letterbox",    None),       # PCR_PLUNE
    18: ("letterbox",    None),       # PCR_ALUNE
    20: ("letterbox",    None),       # PCR_HACIENDA — baked vignette
    22: ("letterbox",    None),       # PCR_VUPHARE
    24: ("letterbox",    None),       # PCR_VUPHARE2 — porthole
    26: ("letterbox",    None),       # planet + comet
    28: ("letterbox",    None),       # Funfrock lab
    30: ("letterbox",    None),       # picnic
    32: ("letterbox",    None),       # Emerald Moon porthole
    34: ("letterbox",    None),       # cell with orbs
    36: ("letterbox",    None),       # celestial diagram
    38: ("letterbox",    None),       # Mona Lisa (mirror-tile was viable but letterbox is consistent)
    40: ("letterbox",    None),       # volcano paper map
    42: ("letterbox",    None),       # volcano slate map
    44: ("letterbox",    None),       # album + guard
    46: ("letterbox",    None),       # bust on plinth
    48: ("letterbox",    None),       # musicians
    50: ("letterbox",    None),       # prison cell
    52: ("letterbox",    None),       # dragon-fire
    54: ("letterbox",    None),       # red-sky forum
    56: ("letterbox",    None),       # spaceship + planet
    58: ("letterbox",    None),       # explosion celebration
    60: ("letterbox",    None),       # PCR_CDROM
    62: ("letterbox",    None),       # Sendell baby orb
    64: ("letterbox",    None),       # paper house map
    66: ("letterbox",    None),       # slate house map
    68: ("letterbox",    None),       # Twinsen + medallion
    70: ("letterbox",    None),       # Sendell-form Twinsen
    72: ("palette-fill", "white"),    # PCR_ACTIVISION — white bg, invisible fill
    74: ("letterbox",    None),       # PCR_EA — black bg, identical to letterbox
    76: ("palette-fill", "white"),    # PCR_VIRGIN — white bg, invisible fill
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

def apply_stretch(rgb, target_w):
    """Bilinear horizontal resample 640 -> target_w. Height unchanged."""
    return rgb.resize((target_w, H), Image.BILINEAR)

# Stops sampled from PCR_MENU's stormy-sky palette so the gradient feels like
# the same world: deep midnight at top, mid blue-grey at mid-sky, slate at the
# horizon.  Easy to retune if a different mood is wanted later.
GRADIENT_STOPS = [
    (0.00, (10,  16,  32)),    # near-black sky top
    (0.55, (40,  60,  80)),    # mid blue-grey
    (1.00, (90, 110, 130)),    # slate horizon
]

def apply_gradient(target_w):
    """Replacement treatment: ignore the bitmap, fill with a vertical gradient."""
    canvas = Image.new("RGB", (target_w, H))
    px = canvas.load()
    stops = GRADIENT_STOPS
    for y in range(H):
        t = y / (H - 1)
        # find bracketing stops
        for i in range(len(stops) - 1):
            t0, c0 = stops[i]
            t1, c1 = stops[i + 1]
            if t0 <= t <= t1:
                u = (t - t0) / (t1 - t0) if t1 > t0 else 0
                r = round(c0[0] + (c1[0] - c0[0]) * u)
                g = round(c0[1] + (c1[1] - c0[1]) * u)
                b = round(c0[2] + (c1[2] - c0[2]) * u)
                break
        for x in range(target_w):
            px[x, y] = (r, g, b)
    return canvas

TREATMENTS = {
    "letterbox":    lambda rgb, w, b, p, fi: apply_letterbox(rgb, w),
    "palette-fill": lambda rgb, w, b, p, fi: apply_palette_fill(rgb, w, b, p, fi),
    "edge-clone":   lambda rgb, w, b, p, fi: apply_edge_clone(rgb, w),
    "mirror-tile":  lambda rgb, w, b, p, fi: apply_mirror_tile(rgb, w),
    "stretch":      lambda rgb, w, b, p, fi: apply_stretch(rgb, w),
    "gradient":     lambda rgb, w, b, p, fi: apply_gradient(w),
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

COMPARE_ORDER = ("letterbox", "palette-fill", "edge-clone", "mirror-tile",
                 "stretch", "gradient")

def render_compare(hqr_path, entry, target_w):
    """Every treatment stacked vertically with labels."""
    from PIL import ImageDraw
    bitmap, palette = load_bitmap_pair(hqr_path, entry)
    rgb = to_rgb_image(bitmap, palette)
    label_h = 24
    rows = []
    for name in COMPARE_ORDER:
        img = TREATMENTS[name](rgb, target_w, bitmap, palette, None)
        labelled = Image.new("RGB", (target_w, H + label_h), (32, 32, 32))
        labelled.paste(img, (0, label_h))
        draw = ImageDraw.Draw(labelled)
        draw.text((8, 4), name, fill=(255, 255, 255))
        rows.append(labelled)
    combined = Image.new("RGB", (target_w, (H + label_h) * len(rows)), (0, 0, 0))
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
