// ──────────────────────────────────────────────────────────────────────────
// PNG Screenshot Support (using stb_image_write)
// ──────────────────────────────────────────────────────────────────────────
#include <SYSTEM.H>
#include <SYSTEM/LOGPRINT.H>
#include <SVGA/SCREEN.H>
#include <FILEIO/SAVEPNG.H>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ──────────────────────────────────────────────────────────────────────────
void SavePNG(char *filename, void *screen, S32 xres, S32 yres, void *ptrpalette) {
    U8 *indexed = (U8 *)screen;
    U8 *palette = (U8 *)ptrpalette;
    U8 *rgb;
    S32 x, y;
    S32 idx;
    int result;

    rgb = (U8 *)Malloc(xres * yres * 3);
    if (!rgb) {
        LogPrintf("Error: Failed to allocate memory for screenshot\n");
        return;
    }

    for (y = 0; y < yres; y++) {
        for (x = 0; x < xres; x++) {
            idx = indexed[y * ModeDesiredX + x];
            rgb[(y * xres + x) * 3 + 0] = palette[idx * 3 + 0];
            rgb[(y * xres + x) * 3 + 1] = palette[idx * 3 + 1];
            rgb[(y * xres + x) * 3 + 2] = palette[idx * 3 + 2];
        }
    }

    result = stbi_write_png(filename, xres, yres, 3, rgb, xres * 3);
    if (result) {
        LogPrintf("Screenshot saved: %s\n", filename);
    } else {
        LogPrintf("Error: Failed to save screenshot to %s\n", filename);
    }

    Free(rgb);
}

// ──────────────────────────────────────────────────────────────────────────
