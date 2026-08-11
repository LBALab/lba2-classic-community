# Graphical quality options

Variables and locations that can be changed or exposed as options to improve graphical quality, increase details and effects, etc.

## Shipped options

All three are off by default, so a stock run renders exactly as before. None of them has a menu entry yet; set them from the console or the config file.

| Option | Console | lba2.cfg | Environment | Values |
|--------|---------|----------|-------------|--------|
| Texture filtering | `gfx_texfilter` | `TextureFilter` | `LBA2_TEXFILTER` | 0 off, 1 horizontal 2-tap, 2 bilinear 4-tap |
| Dithered shading | `gfx_dither` | `DitherShading` | (none) | 0 off, 1 on |
| Interior render scaling | (none) | (none) | `LBA2_ISO_DIV` | 1 off, 2 to 4 |

**Texture filtering** smooths magnified texels on terrain, sea, and sky. The rasterizer works in palette indices, where averaging two entries is meaningless, so the blend is a precomputed table of the nearest palette index to each 25/50/75% RGB mix. Costs roughly 7% of frame time on terrain and 4% on a sea-heavy view at 1728x1080 with the 4-tap setting.

**Dithered shading** applies an ordered dither to Gouraud shade rows, which softens the 16-step banding of the original palette ramps.

**Interior render scaling** renders isometric interiors at 1/N of the chosen resolution and lets the present stretch them back, so the room is drawn larger. Nothing in an interior scales with the framebuffer (bricks are pre-rendered sprites and the iso projectors take no focal argument), so a bigger framebuffer otherwise just reveals more empty space around the room: it covers 29% of the frame at 1728x1080 against 82% at 640x480. Exteriors, whose projection does carry a focal, always render at full resolution.

`LBA2_ISO_DIV` is read once at startup and cannot be changed mid-run. It is also ignored when the divided size would fall below the engine's 320x200 minimum, which is silent: at 640x480 a divisor of 2 works and 3 does nothing.

```
LBA2_ISO_DIV=2 ./lba2cc
```

## Ideas not yet wired up

- DetailLevel and Shadow are stored in lba2.cfg and exposed via the Options menu; see [CONFIG.md](CONFIG.md) for the full key list.
- `RAIN_RANGE` in `SOURCES/3DEXT/LINERAIN.ASM` controls how far the rain effect is drawn in the original ASM implementation.
- In the community build, the rain line routine is compiled from `SOURCES/3DEXT/LINERAIN.CPP` by default; the ASM file is kept for historical reference. If you want a user-facing option for rain draw distance in modern builds, consider wiring it through the C++ implementation rather than only tweaking the ASM constant.
