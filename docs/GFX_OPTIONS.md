# Graphical quality options

Variables and locations that can be changed or exposed as options to improve graphical quality, increase details and effects, etc.

## Shipped options

All four are off by default, so a stock run renders exactly as before. None of them has a menu entry yet; set them from the console or the config file.

| Option | Console | lba2.cfg | Environment | Values |
|--------|---------|----------|-------------|--------|
| Texture filtering | `gfx_texfilter` | `TextureFilter` | `LBA2_TEXFILTER` | 0 off, 1 horizontal 2-tap, 2 bilinear 4-tap |
| Dithered shading | `gfx_dither` | `DitherShading` | (none) | 0 off, 1 on |
| Interior render scaling | (none) | (none) | `LBA2_ISO_DIV` | 1 off, 2 to 4 |
| Present filter | `gfx_presentfilter` | `PresentFilter` | `LBA2_PRESENT_FILTER` | 0 nearest, 1 linear, 2 pixelart |

**Texture filtering** smooths magnified texels on terrain, sea, and sky. The rasterizer works in palette indices, where averaging two entries is meaningless, so the blend is a precomputed table of the nearest palette index to each 25/50/75% RGB mix. Costs roughly 7% of frame time on terrain and 4% on a sea-heavy view at 1728x1080 with the 4-tap setting.

**Dithered shading** applies an ordered dither to Gouraud shade rows, which softens the 16-step banding of the original palette ramps.

**Interior render scaling** renders isometric interiors at 1/N of the chosen resolution and lets the present stretch them back, so the room is drawn larger. Nothing in an interior scales with the framebuffer (bricks are pre-rendered sprites and the iso projectors take no focal argument), so a bigger framebuffer otherwise just reveals more empty space around the room: it covers 29% of the frame at 1728x1080 against 82% at 640x480. Exteriors, whose projection does carry a focal, always render at full resolution.

`LBA2_ISO_DIV` is read once at startup and cannot be changed mid-run. It is also ignored when the divided size would fall below the engine's 320x200 minimum, which is silent: at 640x480 a divisor of 2 works and 3 does nothing.

```
LBA2_ISO_DIV=2 ./lba2cc
```

**Present filter** chooses the filter that stretches the finished frame to the window. Nearest is the original output, and it is uneven whenever the window is not a whole multiple of the resolution: 1728x1080 on a 3840x2400 display is 20/9, so a source pixel is two output pixels wide in one place and three in the next. Linear removes that by blurring everything, the HUD and text included. Pixelart keeps the inside of each pixel sharp and blends only the seam between two of them.

Interior render scaling relies on this same stretch to bring a divided interior back to the window, so the filter also decides how those rooms look.

Pixelart needs SDL 3.4 or newer; a build or a runtime older than that logs a warning and presents with nearest, and the setting is kept for a build that can do it. SDL's software renderer draws pixelart exactly as nearest, so it only shows on a GPU renderer. The console changes the mode on the frame being shown, which makes it the quickest way to compare:

```
gfx_presentfilter 2
```

`LBA2_PRESENT_FILTER` takes the names as well as the numbers, applies to one run and is not written to `lba2.cfg`. A value it does not know is reported in the log and ignored.

```
LBA2_PRESENT_FILTER=pixelart ./lba2cc
```

## Ideas not yet wired up

- DetailLevel and Shadow are stored in lba2.cfg and exposed via the Options menu; see [CONFIG.md](CONFIG.md) for the full key list.
- `RAIN_RANGE` in `SOURCES/3DEXT/LINERAIN.ASM` controls how far the rain effect is drawn in the original ASM implementation.
- In the community build, the rain line routine is compiled from `SOURCES/3DEXT/LINERAIN.CPP` by default; the ASM file is kept for historical reference. If you want a user-facing option for rain draw distance in modern builds, consider wiring it through the C++ implementation rather than only tweaking the ASM constant.
