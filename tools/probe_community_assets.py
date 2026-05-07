#!/usr/bin/env python3
"""
Probe PNGs under assets/ and optionally export engine-sized samples.

The retail game shows full-screen startup art from screen.hqr as:
  - raw indexed pixels (LIM-style): typically 640 x 480 bytes for full-screen
  - palette (PAL): 768 bytes RGB x256

The SDL renderer still presents an 8-bit indexed framebuffer for this path;
true-colour PNGs must be downscaled and quantized for drop-in replacement of
Load_HQR(..., Log, ...) unless the engine gains an RGBA blit on startup.

Dependencies: Pillow (pip install pillow).

Examples:
  python3 tools/probe_community_assets.py
  python3 tools/probe_community_assets.py --export --fit cover
  python3 tools/probe_community_assets.py --export --write-pcx
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def ensure_pil():
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        print(
            "Pillow is required. Install with: pip install pillow",
            file=sys.stderr,
        )
        sys.exit(1)
    from PIL import Image

    return Image


def list_pngs(assets_dir: Path) -> list[Path]:
    if not assets_dir.is_dir():
        print(f"No assets directory at {assets_dir}", file=sys.stderr)
        return []
    return sorted(p for p in assets_dir.iterdir() if p.suffix.lower() == ".png")


def probe_image(Image, path: Path) -> None:
    im = Image.open(path)
    print(f"\n{path.name}")
    print(f"  size: {im.size[0]} x {im.size[1]}")
    print(f"  mode: {im.mode}")
    if im.mode in ("P", "RGB", "RGBA"):
        print(f"  has transparency: {im.mode == 'RGBA' or 'transparency' in im.info}")


def resize_for_engine(
    Image,
    im,
    target_w: int,
    target_h: int,
    fit: str,
    content_scale: float = 1.0,
):
    """Return a new RGB image target_w x target_h using fit=cover|contain|stretch.

    content_scale: multiply the fitted bitmap dimensions by this factor (0..1]
    before centering on the canvas, so e.g. 0.85 leaves an extra margin even when
    aspect ratio matches 4:3 (contain would otherwise fill the frame).
    """
    tw, th = target_w, target_h
    if content_scale <= 0 or content_scale > 1:
        raise ValueError("content_scale must be in (0, 1]")

    if fit == "stretch":
        rgb = im.convert("RGB").resize((tw, th), Image.Resampling.LANCZOS)
        if content_scale < 1.0:
            nw = max(1, int(round(tw * content_scale)))
            nh = max(1, int(round(th * content_scale)))
            rgb = rgb.resize((nw, nh), Image.Resampling.LANCZOS)
            canvas = Image.new("RGB", (tw, th), (0, 0, 0))
            canvas.paste(rgb, ((tw - nw) // 2, (th - nh) // 2))
            return canvas
        return rgb

    src = im.convert("RGBA")
    sw, sh = src.size
    scale = max(tw / sw, th / sh) if fit == "cover" else min(tw / sw, th / sh)
    nw = max(1, int(round(sw * scale)))
    nh = max(1, int(round(sh * scale)))
    resized = src.resize((nw, nh), Image.Resampling.LANCZOS)

    if content_scale < 1.0:
        nw = max(1, int(round(nw * content_scale)))
        nh = max(1, int(round(nh * content_scale)))
        resized = resized.resize((nw, nh), Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (tw, th), (0, 0, 0, 255))
    ox = (tw - nw) // 2
    oy = (th - nh) // 2
    canvas.paste(resized, (ox, oy), resized)
    return canvas.convert("RGB")


def export_engine_assets(
    Image,
    src: Path,
    out_dir: Path,
    fit: str,
    write_pcx: bool,
    content_scale: float = 1.0,
) -> None:
    im = Image.open(src)
    rgb = resize_for_engine(Image, im, 640, 480, fit=fit, content_scale=content_scale)
    # Pillow 8–11: optional method=Image.MEDIANCUT / Image.Quantize.MEDIANCUT
    q = rgb.quantize(colors=256)
    pal = q.getpalette()
    if pal is None or len(pal) < 768:
        print(f"  skip {src.name}: quantization failed", file=sys.stderr)
        return

    stem = src.stem
    lim_path = out_dir / f"{stem}_640x480.lim"
    pal_path = out_dir / f"{stem}_640x480.pal"

    lim_bytes = q.tobytes()
    if len(lim_bytes) != 640 * 480:
        print(
            f"  unexpected LIM size {len(lim_bytes)} for {src.name}",
            file=sys.stderr,
        )
        return

    pal_bytes = bytes(pal[:768])
    out_dir.mkdir(parents=True, exist_ok=True)
    lim_path.write_bytes(lim_bytes)
    pal_path.write_bytes(pal_bytes)

    print(f"  wrote {lim_path.relative_to(repo_root())} ({len(lim_bytes)} bytes)")
    print(f"  wrote {pal_path.relative_to(repo_root())} ({len(pal_bytes)} bytes)")

    if write_pcx:
        pcx_path = out_dir / f"{stem}_640x480.pcx"
        q.save(str(pcx_path), format="PCX")
        print(f"  wrote {pcx_path.relative_to(repo_root())}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--assets-dir",
        type=Path,
        default=None,
        help="Directory containing PNGs (default: <repo>/assets)",
    )
    parser.add_argument(
        "--export",
        action="store_true",
        help="Write 640x480 .lim/.pal under assets/generated/",
    )
    parser.add_argument(
        "--fit",
        choices=("cover", "contain", "stretch"),
        default="cover",
        help="How to map source aspect ratio to 4:3 framebuffer",
    )
    parser.add_argument(
        "--write-pcx",
        action="store_true",
        help="Also write quantised PCX (still 256 colours)",
    )
    parser.add_argument(
        "--content-scale",
        type=float,
        default=1.0,
        metavar="S",
        help="Shrink fitted artwork by this factor (0..1] before centering; "
        "default 1 = full fitted size (use with 4:3 assets that would otherwise fill the frame)",
    )
    args = parser.parse_args()
    if args.content_scale <= 0 or args.content_scale > 1:
        print("error: --content-scale must be in (0, 1]", file=sys.stderr)
        sys.exit(1)

    root = repo_root()
    assets_dir = args.assets_dir if args.assets_dir else root / "assets"
    Image = ensure_pil()

    pngs = list_pngs(assets_dir)
    if not pngs:
        sys.exit(1)

    print(f"Assets directory: {assets_dir}")
    print("Engine target for Load_HQR full-screen stills: 640 x 480 indexed + 768-byte RGB palette.")

    for p in pngs:
        probe_image(Image, p)

    if args.export:
        out_dir = root / "assets" / "generated"
        print(
            f"\nExporting to {out_dir} (fit={args.fit}, content_scale={args.content_scale})..."
        )
        for p in pngs:
            print(f"\n{p.name}:")
            export_engine_assets(
                Image,
                p,
                out_dir,
                args.fit,
                args.write_pcx,
                content_scale=args.content_scale,
            )

        print(
            "\nNote: PAL order is RGB triplets for indices 0..255, matching Pillow getpalette()."
        )
        print(
            "Verify once in-game (some DOS assets use bottom-up scan order; flip if the image is upside down)."
        )
        print(
            "\nFor embedded DistribLogo splash, run:\n"
            "  python3 tools/prepare_embedded_startup_assets.py\n"
            "(see assets/embedded/README.txt)"
        )


if __name__ == "__main__":
    main()
