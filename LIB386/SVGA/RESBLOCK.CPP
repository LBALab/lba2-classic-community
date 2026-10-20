#include <SVGA/RESBLOCK.H>

#include <string.h>

#include <SVGA/SCREEN.H>
#include <SYSTEM/DEBUG_TRACES.H>

void RestoreBlock(void *screen, void *buffer, S32 x0, S32 y0, S32 x1, S32 y1) {
    LIB386_TRACE_CPP("RestoreBlock", "1", "scr=%p buf=%p x0=%d y0=%d x1=%d y1=%d", screen, buffer, x0, y0, x1, y1);
    // An inverted rectangle copies nothing, as RESBLOCK.ASM does for
    // x1 == x0 - 1 (zero rep counts). Inverted further, or by a row, the
    // ASM's counts wrap to about 4 G and the copy runs past both buffers.
    if (x1 < x0 || y1 < y0) {
        return;
    }

    // The buffer holds the whole rectangle, x1 - x0 + 1 bytes a row, as
    // SaveBlock wrote it. Only the part on screen is drawn. The ASM does not
    // clip; its callers keep the rectangle on screen.
    const S32 maxX = (S32)ModeDesiredX - 1;
    const S32 maxY = (S32)ModeDesiredY - 1;
    if (x1 < 0 || y1 < 0 || x0 > maxX || y0 > maxY) {
        return;
    }
    const S32 cx0 = x0 < 0 ? 0 : x0;
    const S32 cy0 = y0 < 0 ? 0 : y0;
    const S32 cx1 = x1 > maxX ? maxX : x1;
    const S32 cy1 = y1 > maxY ? maxY : y1;

    // Unsigned, so x1 - x0 cannot overflow for extreme coordinates.
    const U32 stride = (U32)x1 - (U32)x0 + 1u;
    const U32 width = (U32)(cx1 - cx0 + 1);
    U32 rows = (U32)(cy1 - cy0 + 1);

    const U8 *src = (const U8 *)buffer + (size_t)((U32)cy0 - (U32)y0) * stride + ((U32)cx0 - (U32)x0);
    U8 *dst = (U8 *)screen + TabOffLine[cy0] + cx0;

    do {
        memcpy(dst, src, width);
        src += stride;
        dst += TabOffLine[1];
    } while (--rows);
}
