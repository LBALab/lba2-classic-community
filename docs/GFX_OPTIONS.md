# Graphical quality options

Variables and locations that can be changed or exposed as options to improve graphical quality, increase details and effects, etc.

- DetailLevel and Shadow are stored in lba2.cfg and exposed via the Options menu; see [CONFIG.md](CONFIG.md) for the full key list.
- `RAIN_RANGE` in `SOURCES/3DEXT/LINERAIN.ASM` controls how far the rain effect is drawn in the original ASM implementation.
- In the community build, the rain line routine is compiled from `SOURCES/3DEXT/LINERAIN.CPP` by default; the ASM file is kept for historical reference. If you want a user-facing option for rain draw distance in modern builds, consider wiring it through the C++ implementation rather than only tweaking the ASM constant.
