#ifndef IMAGE_LOAD_H
#define IMAGE_LOAD_H

/*
 * Stride-aware loaders for the 640x480 raw bitmaps that ship inside
 * screen.hqr and friends.
 *
 * Each HQR entry consumed here is exactly 640 * 480 = 307200 bytes of
 * palette-indexed pixel data — the PCX source files were decoded at
 * HQR-build time into raw byte blocks (no PCX format at runtime).
 *
 * Background: `Load_HQR(file, dest, index)` writes the resource bytes into
 * <dest> at offset 0 with no stride awareness. When the live framebuffer
 * (Log / Screen) is wider — i.e. its row stride is `ModeDesiredX` bytes,
 * not 640 — a raw `Load_HQR(..., Log, index)` shears the image: byte
 * N*640 of the bitmap lands at framebuffer column `N * 640 % ModeDesiredX`
 * instead of the top of row N. At 1024x768 the result is a sheared band of
 * content in the upper-left of the frame.
 *
 * The two helpers below load through an internal 640x480 staging buffer
 * (allocated alongside the rest of `ListMem` in MEM.CPP), then either
 * row-copy it centred or nearest-neighbour stretch it. At 640x480 both
 * helpers degenerate to the same byte layout as the historical
 * `Load_HQR(..., dest, index)` call — no behaviour change at the canonical
 * width.
 *
 * Non-bitmap `Load_HQR` calls (palettes, animation data, scene scripts) are
 * unaffected — they don't go through these helpers.
 */

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* The fixed authored canvas size — every bitmap in screen.hqr is 640x480. */
#define IMAGE_AUTHORED_WIDTH 640
#define IMAGE_AUTHORED_HEIGHT 480
#define IMAGE_STAGING_BYTES ((U32)(IMAGE_AUTHORED_WIDTH * IMAGE_AUTHORED_HEIGHT))

/* Persistent 640x480 staging buffer (IMAGE_STAGING_BYTES + RECOVER_AREA).
   Owned by the main memory pool — see MEM.CPP's ListMem table. Don't free. */
extern U8 *ImageStaging;

/* Load <file> resource <index> as a 640x480 raw bitmap into the staging
   buffer, then row-copy each scanline into <dest> centred horizontally and
   vertically within (ModeDesiredX, ModeDesiredY). Pixels outside the 640x480
   inscribed rectangle are left untouched — callers that need the sidebars
   black should clear <dest> first. Returns the loaded byte count (0 on
   failure). */
U32 Image_LoadCentered(const char *file, void *dest, S32 index);

/* Same as Image_LoadCentered but nearest-neighbour stretches the 640x480
   content to fill the entire (ModeDesiredX, ModeDesiredY) area of <dest>. */
U32 Image_LoadStretched(const char *file, void *dest, S32 index);

/* Row-copy the staging buffer (whatever's currently in it) centred into
   <dest>, without re-loading. Used by transitions / fades that read
   repeatedly from a previously-loaded bitmap. */
void Image_RenderCenteredFromStaging(void *dest);

#ifdef __cplusplus
}
#endif

#endif /* IMAGE_LOAD_H */
