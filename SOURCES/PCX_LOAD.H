#ifndef PCX_LOAD_H
#define PCX_LOAD_H

/*
 * PCX-style screen loaders that respect a runtime framebuffer wider than the
 * authored 640x480 PCX content.
 *
 * Background: `Load_HQR(file, dest, index)` writes the resource bytes into
 * <dest> at offset 0 with no stride awareness. PCX frames in `screen.hqr`
 * are exactly 640x480 = 307200 bytes. When the live framebuffer (Log /
 * Screen) is wider — i.e. its row stride is `ModeDesiredX` bytes, not 640 —
 * a raw `Load_HQR(..., Log, PCR_FOO)` shears the image: byte N*640 of the
 * PCX lands at framebuffer column `N * 640 % ModeDesiredX` instead of the
 * top of row N. At 1024x768 the result is a sheared band of PCX content in
 * the upper-left of the frame.
 *
 * The two helpers below load the PCX through an internal 640x480 staging
 * buffer (allocated alongside the rest of `ListMem` in MEM.CPP), then either
 * row-copy it centred or nearest-neighbour stretch it. At 640x480 both
 * helpers degenerate to the same byte layout as the historical
 * `Load_HQR(..., dest, index)` call — no behaviour change at the canonical
 * width.
 *
 * Non-PCX `Load_HQR` calls (palettes, animation data, scene scripts) are
 * unaffected — they don't go through these helpers.
 */

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* The fixed PCX authored canvas size — every PCX in screen.hqr is 640x480. */
#define PCX_AUTHORED_WIDTH 640
#define PCX_AUTHORED_HEIGHT 480
#define PCX_STAGING_BYTES ((U32)(PCX_AUTHORED_WIDTH * PCX_AUTHORED_HEIGHT))

/* Persistent 640x480 staging buffer (PCX_STAGING_BYTES + RECOVER_AREA). Owned
   by the main memory pool — see MEM.CPP's ListMem table. Don't free. */
extern U8 *PcxStaging;

/* Load <file> resource <index> as a 640x480 PCX into the staging buffer, then
   row-copy each scanline into <dest> centred horizontally and vertically
   within (ModeDesiredX, ModeDesiredY). Pixels outside the 640x480 inscribed
   rectangle are left untouched — callers that need the sidebars black should
   clear <dest> first. Returns the loaded byte count (0 on failure). */
U32 Pcx_LoadCentered(const char *file, void *dest, S32 index);

/* Same as Pcx_LoadCentered but nearest-neighbour stretches the 640x480
   content to fill the entire (ModeDesiredX, ModeDesiredY) area of <dest>. */
U32 Pcx_LoadStretched(const char *file, void *dest, S32 index);

/* Row-copy the staging buffer (whatever's currently in it) centred into
   <dest>, without re-loading. Used by transitions / fades that read repeatedly
   from a previously-loaded PCX. */
void Pcx_RenderCenteredFromStaging(void *dest);

#ifdef __cplusplus
}
#endif

#endif /* PCX_LOAD_H */
