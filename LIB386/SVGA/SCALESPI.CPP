
#include <SVGA/SCALESPI.H>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>
#include <SYSTEM/DEBUG_TRACES.H>

/* Incremented by the software sprite-scale paths; printed by the game's
   per-frame draw census. */
extern "C" U32 s_censusScaleLog = 0;

#ifdef USE_GPURENDERER
#include <GL/RENDER_GL.H>
/* Set by the gameplay object pass (SOURCES/OBJECT.CPP): the aZ depth of the
   sprite being drawn.  Zero means "no GL sprite quad requested" — menus and
   interior blits keep the Log path. */
extern U16 SpriteGL_Zo;
#endif

// Sprite header — matches the ASM struct (SCALESPI.ASM).
typedef struct {
    U8 Delta_X;
    U8 Delta_Y;
    S8 Hot_X;
    S8 Hot_Y;
} Struc_Sprite;

static void set_exit_bounds(void) {
    ScreenXMin = 32000;
    ScreenXMax = -32000;
    ScreenYMin = 32000;
    ScreenYMax = -32000;
}

static S32 mul_shift16(S32 left, S32 right) {
    return (S32)(((long long)left * (long long)right) >> 16);
}

static S32 divide_16_16(S32 value) {
    return (S32)(((long long)1 << 32) / (long long)value);
}

static S32 sprite_center_16_16(U8 delta) {
    return (S32)(((U32)(delta >> 1) << 16) + ((delta & 1u) ? 0x8000u : 0u));
}

/* Source rectangle for the GL scaled-sprite quad.  The destination is
   `width` pixels wide, but the software walk advances the source once per
   destination pixel, so the sampled span is (width - 1) inverse-scale steps,
   not `width` texels. */
static void gl_scaled_source_rect(S32 startTexX, S32 startTexY,
                                  S32 width, S32 height,
                                  S32 invFactorX, S32 invFactorY,
                                  S32 spriteW, S32 spriteH,
                                  S32 *u0, S32 *v0, S32 *u1, S32 *v1) {
    S32 lastX = startTexX + (S32)((long long)(width - 1) * invFactorX);
    S32 lastY = startTexY + (S32)((long long)(height - 1) * invFactorY);
    *u0 = startTexX >> 16;
    *v0 = startTexY >> 16;
    *u1 = (lastX >> 16) + 1;
    *v1 = (lastY >> 16) + 1;
    /* A negative hotspot or a clipped start can put the source origin
       outside the bank; clamp before it wraps the 8.8 UV. */
    if (*u0 < 0)
        *u0 = 0;
    if (*v0 < 0)
        *v0 = 0;
    if (*u0 >= spriteW)
        *u0 = spriteW - 1;
    if (*v0 >= spriteH)
        *v0 = spriteH - 1;
    if (*u1 <= *u0)
        *u1 = *u0 + 1;
    if (*v1 <= *v0)
        *v1 = *v0 + 1;
    if (*u1 > spriteW)
        *u1 = spriteW;
    if (*v1 > spriteH)
        *v1 = spriteH;
}

#ifdef __cplusplus
extern "C" {
#endif

// Opaque sprite blit with arbitrary 16.16 scale factors.
//
// Same algorithm as ScaleSpriteTransp (which is ASM-equivalent per
// tests/SVGA/test_scalespt.cpp). The only difference is the inner pixel
// write: this one copies the source byte straight to the framebuffer (color
// 0 = transparent sentinel, all others written as-is); ScaleSpriteTransp
// instead blends source with destination through PtrTransPal, the per-level
// 256x256 transparency LUT.
//
// factorx / factory are 16.16 fixed-point. 65536 = 1:1, larger = scale up,
// smaller = scale down. <= 0 follows the ASM convention: 0 returns
// exit-bounds, negative is treated as +INT_MAX (so the sprite collapses to a
// single texel column / row).
void ScaleSprite(S32 num, S32 x, S32 y, S32 factorx, S32 factory, void *ptrbank) {
    LIB386_TRACE_CPP("ScaleSprite", "1", "num=%d x=%d y=%d fx=%d fy=%d bank=%p", num, x, y, factorx, factory, (void *)ptrbank);

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
    Struc_Sprite *sprite = (Struc_Sprite *)spritePtr;
    U8 *srcBase = spritePtr + sizeof(Struc_Sprite);
    const S32 clipXMaxPlusOne = ClipXMax + 1;
    const S32 clipYMaxPlusOne = ClipYMax + 1;

    // Fast path: 1:1 — straight copy with color-0 transparency.
    if (factorx == 65536 && factory == 65536) {
        S32 leftX = x + (S8)sprite->Hot_X;
        S32 topY = y + (S8)sprite->Hot_Y;
        S32 rightX = leftX + sprite->Delta_X;
        S32 bottomY = topY + sprite->Delta_Y;

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

#ifdef USE_GPURENDERER
        if (SpriteGL_Zo != 0) {
            GL_DrawSpriteQuad(ScreenXMin, ScreenYMin,
                              ScreenXMin + width, ScreenYMin + height,
                              startX, startY,
                              startX + width, startY + height,
                              srcBase, sprite->Delta_X, sprite->Delta_Y,
                              SpriteGL_Zo, 0);
            SpriteGL_Zo = 0;
            return;
        }
#endif
        s_censusScaleLog++;
        U8 *dstPtr = (U8 *)Log + TabOffLine[ScreenYMin] + ScreenXMin;
        U8 *srcPtr = srcBase + startY * sprite->Delta_X + startX;

        S32 srcSkip = sprite->Delta_X - width;
        S32 dstSkip = ModeDesiredX - width;

        for (S32 scanY = 0; scanY < height; scanY++) {
            for (S32 scanX = 0; scanX < width; scanX++) {
                U8 srcPixel = *srcPtr++;
                if (srcPixel != 0) {
                    *dstPtr = srcPixel;
                }
                dstPtr++;
            }
            srcPtr += srcSkip;
            dstPtr += dstSkip;
        }
        return;
    }

    // Scaled path: walk destination in screen space, source in 16.16 texel
    // space. Per-pixel/row carry advances the source pointer.
    S32 newDeltaX = mul_shift16(sprite->Delta_X, factorx);
    S32 newDeltaY = mul_shift16(sprite->Delta_Y, factory);
    S32 newHotX = mul_shift16((S8)sprite->Hot_X, factorx);
    S32 newHotY = mul_shift16((S8)sprite->Hot_Y, factory);
    S32 invFactorX = divide_16_16(factorx);
    S32 invFactorY = divide_16_16(factory);
    S32 startTexX = sprite_center_16_16(sprite->Delta_X) - invFactorX * (newDeltaX >> 1);
    S32 startTexY = sprite_center_16_16(sprite->Delta_Y) - invFactorY * (newDeltaY >> 1);

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

#ifdef USE_GPURENDERER
    if (SpriteGL_Zo != 0) {
        S32 u0, v0, u1, v1;
        gl_scaled_source_rect(startTexX, startTexY, width, height,
                              invFactorX, invFactorY,
                              sprite->Delta_X, sprite->Delta_Y,
                              &u0, &v0, &u1, &v1);
        GL_DrawSpriteQuad(ScreenXMin, ScreenYMin,
                          ScreenXMin + width, ScreenYMin + height,
                          u0, v0, u1, v1,
                          srcBase, sprite->Delta_X, sprite->Delta_Y,
                          SpriteGL_Zo, 0);
        SpriteGL_Zo = 0;
        return;
    }
#endif
    s_censusScaleLog++;
    U8 *dstPtr = (U8 *)Log + TabOffLine[ScreenYMin] + ScreenXMin;

    U8 *srcPtr = srcBase + ((U32)startTexY >> 16) * sprite->Delta_X;
    U32 xFrac = ((U32)startTexX) << 16;
    U32 yFrac = ((U32)startTexY) << 16;
    U32 xDec = ((U32)invFactorX) << 16;
    U32 yDec = ((U32)invFactorY) << 16;
    U32 xInt = ((U32)invFactorX) >> 16;
    U32 yInt = ((U32)invFactorY) >> 16;
    S32 srcRowWidth = sprite->Delta_X;

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
                *lineDst = srcPixel;
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
