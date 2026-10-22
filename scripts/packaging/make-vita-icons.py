#!/usr/bin/env python3
"""Regenerate committed Vita LiveArea images from packaging/lba2cc.png (Pillow)."""

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "packaging/vita"


def main():
    with Image.open(ROOT / "packaging/lba2cc.png") as source:
        icon = source.convert("RGBA").resize((128, 128), Image.Resampling.LANCZOS)
    icon.quantize(colors=128, method=Image.Quantize.FASTOCTREE).save(OUTPUT / "icon0.png")
    background = Image.new("RGBA", (840, 500), (24, 32, 48, 255))
    background.quantize(colors=128, method=Image.Quantize.FASTOCTREE).save(OUTPUT / "bg.png")
    startup = Image.new("RGBA", (280, 158), (24, 32, 48, 255))
    startup.alpha_composite(icon, (76, 15))
    startup.quantize(colors=128, method=Image.Quantize.FASTOCTREE).save(OUTPUT / "startup.png")


if __name__ == "__main__":
    main()
