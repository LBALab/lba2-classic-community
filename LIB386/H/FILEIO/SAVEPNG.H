// ──────────────────────────────────────────────────────────────────────────
// PNG Screenshot Support (using stb_image_write)
// ──────────────────────────────────────────────────────────────────────────
#ifndef SAVEPNG_H
#define SAVEPNG_H

#include <SYSTEM.H>

extern void SavePNG(char *filename, void *screen, S32 xres, S32 yres, void *ptrpalette);

/* Write an 8-bit-per-channel RGB buffer as a PNG. `rgb` holds xres*yres*3
   bytes, row-major, top-down. */
extern void SavePNGRGB(const char *filename, const void *rgb, S32 xres, S32 yres);

#endif // SAVEPNG_H
