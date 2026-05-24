#include "PCX_LOAD.H"

#include <SVGA/SCREEN.H>
#include <SYSTEM/HQRLOAD.H>

#include <string.h>

U8 *PcxStaging = NULL;

static void Pcx_CopyCenteredFromStaging(void *destVoid) {
    /* Treat staging as a tightly-packed 640x480 buffer; treat dest as a
       ModeDesiredX-stride buffer. Centre the 640x480 content in the
       (ModeDesiredX × ModeDesiredY) destination. At 640x480 the offsets are
       both zero and the loop degenerates to a simple memcpy with stride=640
       on both sides — byte-identical to a raw Load_HQR. */
    U8 *dest = (U8 *)destVoid;
    const S32 destStride = (S32)ModeDesiredX;
    const S32 xOffset = (destStride - PCX_AUTHORED_WIDTH) / 2;
    const S32 yOffset = ((S32)ModeDesiredY - PCX_AUTHORED_HEIGHT) / 2;
    const U8 *src = PcxStaging;
    for (S32 y = 0; y < PCX_AUTHORED_HEIGHT; y++) {
        memcpy(dest + (yOffset + y) * destStride + xOffset,
               src + y * PCX_AUTHORED_WIDTH,
               PCX_AUTHORED_WIDTH);
    }
}

static void Pcx_StretchFromStaging(void *destVoid) {
    /* Nearest-neighbour fill of (ModeDesiredX × ModeDesiredY) from the
       640x480 staging. Pixel-art friendly (matches the rest of the engine's
       blocky look) and stride-aware on both sides. At 640x480 every srcY/srcX
       equals y/x, so the result is byte-identical to a memcpy via stride=640. */
    U8 *dest = (U8 *)destVoid;
    const S32 destW = (S32)ModeDesiredX;
    const S32 destH = (S32)ModeDesiredY;
    const U8 *src = PcxStaging;
    for (S32 y = 0; y < destH; y++) {
        const S32 srcY = (y * PCX_AUTHORED_HEIGHT) / destH;
        const U8 *srcRow = src + srcY * PCX_AUTHORED_WIDTH;
        U8 *destRow = dest + y * destW;
        for (S32 x = 0; x < destW; x++) {
            const S32 srcX = (x * PCX_AUTHORED_WIDTH) / destW;
            destRow[x] = srcRow[srcX];
        }
    }
}

U32 Pcx_LoadCentered(const char *file, void *dest, S32 index) {
    U32 size = Load_HQR(file, PcxStaging, index);
    if (size == 0)
        return 0;
    Pcx_CopyCenteredFromStaging(dest);
    return size;
}

U32 Pcx_LoadStretched(const char *file, void *dest, S32 index) {
    U32 size = Load_HQR(file, PcxStaging, index);
    if (size == 0)
        return 0;
    Pcx_StretchFromStaging(dest);
    return size;
}

void Pcx_RenderCenteredFromStaging(void *dest) {
    Pcx_CopyCenteredFromStaging(dest);
}
