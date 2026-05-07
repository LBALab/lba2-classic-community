#!/usr/bin/env python3
"""
Generate embedded LIM/PAL for one community splash image (DistribLogo).

  * assets/splash.png — full-screen slide (default fit: contain).

Requires Pillow (tools/probe_community_assets.py).

  python3 tools/prepare_embedded_startup_assets.py

CMake expects assets/embedded/community_splash_640x480.{lim,pal}.

AdelineLogo remains retail screen.hqr. CD-ROM insert-disc UI is skipped when
LBA2_SKIP_CDROM_CHECK=ON (default for this port).

Then:

  cmake -B build -DLBA2_EMBEDDED_COMMUNITY_STARTUP=ON
  cmake --build build
"""

from __future__ import annotations

import argparse
import importlib.util
import shutil
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def load_probe_module(root: Path):
    probe_path = root / "tools" / "probe_community_assets.py"
    spec = importlib.util.spec_from_file_location("probe_community_assets", probe_path)
    if spec is None or spec.loader is None:
        print(f"error: cannot load {probe_path}", file=sys.stderr)
        sys.exit(1)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main() -> None:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fit",
        choices=("cover", "contain", "stretch"),
        default="contain",
        help="Map splash.png to 640x480 (default: contain — whole image visible)",
    )
    parser.add_argument(
        "--write-pcx",
        action="store_true",
        help="Also write PCX under assets/generated/",
    )
    parser.add_argument(
        "--splash-png",
        type=Path,
        default=root / "assets" / "splash.png",
        help="Source PNG (default: assets/splash.png)",
    )
    parser.add_argument(
        "--content-scale",
        type=float,
        default=1.0,
        metavar="S",
        help="Shrink fitted artwork by this factor (0..1] before centering; "
        "e.g. 0.85 adds margin even when splash is already 4:3 (default: 1)",
    )
    args = parser.parse_args()

    if args.content_scale <= 0 or args.content_scale > 1:
        print("error: --content-scale must be in (0, 1]", file=sys.stderr)
        sys.exit(1)

    splash = args.splash_png.resolve()
    if not splash.is_file():
        print(f"error: missing splash PNG: {splash}", file=sys.stderr)
        sys.exit(1)

    probe = load_probe_module(root)
    Image = probe.ensure_pil()
    gen = root / "assets" / "generated"
    gen.mkdir(parents=True, exist_ok=True)

    print(
        f"Export splash (fit={args.fit}, content_scale={args.content_scale}): "
        f"{splash.name}"
    )
    probe.export_engine_assets(
        Image,
        splash,
        gen,
        args.fit,
        args.write_pcx,
        content_scale=args.content_scale,
    )

    mappings = [
        (
            gen / f"{splash.stem}_640x480.lim",
            root / "assets" / "embedded" / "community_splash_640x480.lim",
        ),
        (
            gen / f"{splash.stem}_640x480.pal",
            root / "assets" / "embedded" / "community_splash_640x480.pal",
        ),
    ]

    embedded_dir = root / "assets" / "embedded"
    embedded_dir.mkdir(parents=True, exist_ok=True)

    for src, dst in mappings:
        if not src.is_file():
            print(f"error: export did not produce {src}", file=sys.stderr)
            sys.exit(1)
        shutil.copy2(src, dst)
        print(f"Copied {src.name} -> {dst.relative_to(root)}")

    print()
    print("DistribLogo: single splash; AdelineLogo unchanged (screen.hqr).")
    print("CD-ROM UI: skipped when LBA2_SKIP_CDROM_CHECK=ON (cmake default).")
    print()
    print("Next:")
    print("  cmake -B build -DLBA2_EMBEDDED_COMMUNITY_STARTUP=ON")
    print("  cmake --build build")


if __name__ == "__main__":
    main()
