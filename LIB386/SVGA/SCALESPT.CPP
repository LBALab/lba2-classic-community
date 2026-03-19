#include <SVGA/SCALESPT.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>
#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

// Sprite structure - matches the ASM definition
struct SpriteStruct {
    U8 deltaX;
    U8 deltaY;
    S8 hotX;
    S8 hotY;
};

static void set_exit_bounds(void)
{
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
}

static S32 mul_shift16(S32 left, S32 right)
{
    return (S32)(((long long)left * (long long)right) >> 16);
}

static S32 divide_16_16(S32 value)
{
    return (S32)(((long long)1 << 32) / (long long)value);
}

static S32 sprite_center_16_16(U8 delta)
{
    return (S32)(((U32)(delta >> 1) << 16) + ((delta & 1u) ? 0x8000u : 0u));
}

#ifdef __cplusplus
extern "C" {
#endif

void ScaleSpriteTransp(S32 num, S32 x, S32 y, S32 factorx, S32 factory,
                       void *ptrbank, void *ptr_transp)
{
    LIB386_TRACE_CPP("ScaleSpriteTransp", "1", "num=%d x=%d y=%d fx=%d fy=%d", num, x, y, factorx, factory);
    if (factorx <= 0) {
        if (factorx == 0) {
            set_exit_bounds();
            return;
        }
        factorx = 0x7FFFFFFF;
    }
    if (factory <= 0) {
        if (factory == 0) {
            set_exit_bounds();
            return;
        }
        factory = 0x7FFFFFFF;
    }

    U32 *bank = (U32 *)ptrbank;
    U8 *spritePtr = (U8 *)ptrbank + bank[num];
    SpriteStruct *sprite = (SpriteStruct *)spritePtr;
    U8 *transpTable = (U8 *)ptr_transp;
    U8 *srcBase = spritePtr + sizeof(SpriteStruct);
    const S32 clipXMaxPlusOne = ClipXMax + 1;
    const S32 clipYMaxPlusOne = ClipYMax + 1;
    
    // Fast mode: 1:1 scaling
    if (factorx == 65536 && factory == 65536) {
        S32 leftX = x + (S8)sprite->hotX;
        S32 topY = y + (S8)sprite->hotY;
        S32 rightX = leftX + sprite->deltaX;
        S32 bottomY = topY + sprite->deltaY;

        if (leftX > clipXMaxPlusOne || topY > clipYMaxPlusOne) {
            set_exit_bounds();
            return;
        }

        ScreenXMin = leftX;
        ScreenYMin = topY;
        S32 startX = 0, startY = 0;

        if (leftX < ClipXMin) {
            startX = ClipXMin - leftX;
            ScreenXMin = ClipXMin;
        }

        if (topY <= ClipYMin) {
            startY = ClipYMin - topY;
            ScreenYMin = ClipYMin;
        }

        if (rightX < ClipXMin) {
            set_exit_bounds();
            return;
        }
        ScreenXMax = rightX;
        if (rightX >= clipXMaxPlusOne) {
            ScreenXMax = clipXMaxPlusOne;
        }

        if (bottomY < ClipYMin) {
            set_exit_bounds();
            return;
        }
        ScreenYMax = bottomY;
        if (bottomY >= clipYMaxPlusOne) {
            ScreenYMax = clipYMaxPlusOne;
        }

        S32 width = ScreenXMax - ScreenXMin;
        S32 height = ScreenYMax - ScreenYMin;

        if (width <= 0 || height <= 0) {
            set_exit_bounds();
            return;
        }

        U8 *dstPtr = (U8 *)Log + TabOffLine[ScreenYMin] + ScreenXMin;
        U8 *srcPtr = srcBase + startY * sprite->deltaX + startX;

        S32 srcSkip = sprite->deltaX - width;
        S32 dstSkip = ModeDesiredX - width;

        for (S32 scanY = 0; scanY < height; scanY++) {
            for (S32 scanX = 0; scanX < width; scanX++) {
                U8 srcPixel = *srcPtr++;
                if (srcPixel != 0) {
                    U8 dstPixel = *dstPtr;
                    U16 blendIndex = (srcPixel << 8) | dstPixel;
                    *dstPtr = transpTable[blendIndex];
                }
                dstPtr++;
            }
            srcPtr += srcSkip;
            dstPtr += dstSkip;
        }
        return;
    }

    // Scaled mode
    S32 newDeltaX = mul_shift16(sprite->deltaX, factorx);
    S32 newDeltaY = mul_shift16(sprite->deltaY, factory);
    S32 newHotX = mul_shift16((S8)sprite->hotX, factorx);
    S32 newHotY = mul_shift16((S8)sprite->hotY, factory);
    S32 invFactorX = divide_16_16(factorx);
    S32 invFactorY = divide_16_16(factory);
    S32 startTexX = sprite_center_16_16(sprite->deltaX) - invFactorX * (newDeltaX >> 1);
    S32 startTexY = sprite_center_16_16(sprite->deltaY) - invFactorY * (newDeltaY >> 1);

    S32 leftX = x + newHotX;
    S32 topY = y + newHotY;
    S32 rightX = leftX + newDeltaX;
    S32 bottomY = topY + newDeltaY;

    ScreenXMin = leftX;
    if (leftX > clipXMaxPlusOne) {
        set_exit_bounds();
        return;
    }

    if (leftX < ClipXMin) {
        ScreenXMin = ClipXMin;
        S32 clipDelta = (ClipXMin - 1) - leftX;
        S32 adjusted = startTexX + clipDelta * invFactorX;
        startTexX += adjusted;
    }

    if (rightX < ClipXMin) {
        set_exit_bounds();
        return;
    }
    ScreenXMax = rightX;
    if (rightX >= clipXMaxPlusOne) {
        ScreenXMax = clipXMaxPlusOne;
    }

    ScreenYMin = topY;
    if (topY > clipYMaxPlusOne) {
        set_exit_bounds();
        return;
    }

    if (topY < ClipYMin) {
        ScreenYMin = ClipYMin;
        S32 clipDelta = (ClipYMin - 1) - topY;
        S32 adjusted = startTexY + clipDelta * invFactorY;
        startTexY += adjusted;
    }

    if (bottomY <= ClipYMin) {
        set_exit_bounds();
        return;
    }
    ScreenYMax = bottomY;
    if (bottomY > clipYMaxPlusOne) {
        ScreenYMax = clipYMaxPlusOne;
    }

    S32 height = ScreenYMax - ScreenYMin;
    S32 width = ScreenXMax - ScreenXMin;
    if (height <= 0 || width <= 0) {
        set_exit_bounds();
        return;
    }

    U8 *dstPtr = (U8 *)Log + TabOffLine[ScreenYMin] + ScreenXMin;

    U8 *srcPtr = srcBase + ((U32)startTexY >> 16) * sprite->deltaX;
    U32 xFrac = ((U32)startTexX) << 16;
    U32 yFrac = ((U32)startTexY) << 16;
    U32 xDec = ((U32)invFactorX) << 16;
    U32 yDec = ((U32)invFactorY) << 16;
    U32 xInt = ((U32)invFactorX) >> 16;
    U32 yInt = ((U32)invFactorY) >> 16;
    S32 srcRowWidth = sprite->deltaX;

    srcPtr += ((U32)startTexX >> 16);

    U32 pendingRows = 0;
    for (S32 scanY = height; scanY != 0; --scanY) {
        S32 remaining = width;
        U8 *lineSrc = srcPtr;
        U8 *lineDst = dstPtr;
        U32 lineFrac = xFrac;

        while (remaining-- != 0) {
            U8 srcPixel = *lineSrc;
            if (srcPixel != 0) {
                U8 dstPixel = *lineDst;
                *lineDst = transpTable[(srcPixel << 8) | dstPixel];
            }

            U32 prevFrac = lineFrac;
            lineFrac += xDec;
            lineSrc += xInt;
            if (lineFrac < prevFrac) {
                ++lineSrc;
            }
            ++lineDst;
        }

        U32 prevYFrac = yFrac;
        yFrac += yDec;
        pendingRows += yInt;
        if (yFrac < prevYFrac) {
            ++pendingRows;
        }

        if (pendingRows != 0) {
            srcPtr += srcRowWidth * pendingRows;
            pendingRows = 0;
        }

        dstPtr += ModeDesiredX;
    }
}

#ifdef __cplusplus
}
#endif
