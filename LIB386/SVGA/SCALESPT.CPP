#include <SVGA/SCALESPI.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

// Sprite structure - matches the ASM definition
struct SpriteStruct {
    U8 deltaX;
    U8 deltaY;
    S8 hotX;
    S8 hotY;
};

// Global variables from ASM
extern "C" {
    extern S32 ScreenXMin, ScreenXMax, ScreenYMin, ScreenYMax;
}

#ifdef __cplusplus
extern "C" {
#endif

void ScaleSpriteTransp(S32 num, S32 x, S32 y, S32 factorx, S32 factory,
                       void *ptrbank, void *ptr_transp)
{
    LIB386_TRACE_CPP("ScaleSpriteTransp", "1", "num=%d x=%d y=%d fx=%d fy=%d", num, x, y, factorx, factory);
    if (factorx <= 0) {
        if (factorx == 0) return;
        factorx = 0x7FFFFFFF;
    }
    if (factory <= 0) {
        if (factory == 0) return;
        factory = 0x7FFFFFFF;
    }
    
    U32 *bank = (U32 *)ptrbank;
    U8 *spritePtr = (U8 *)ptrbank + bank[num];
    SpriteStruct *sprite = (SpriteStruct *)spritePtr;
    
    // Fast mode: 1:1 scaling
    if (factorx == 65536 && factory == 65536) {
        S32 leftX = x + (S8)sprite->hotX;
        S32 topY = y + (S8)sprite->hotY;
        S32 rightX = leftX + sprite->deltaX;
        S32 bottomY = topY + sprite->deltaY;
        
        if (leftX > ClipXMax || topY > ClipYMax) return;
        
        ScreenXMin = leftX;
        ScreenYMin = topY;
        S32 startX = 0, startY = 0;
        
        if (leftX < ClipXMin) {
            startX = ClipXMin - leftX;
            ScreenXMin = ClipXMin;
        }
        
        if (topY < ClipYMin) {
            startY = ClipYMin - topY;
            ScreenYMin = ClipYMin;
        }
        
        ScreenXMax = rightX;
        if (rightX > ClipXMax) {
            ScreenXMax = ClipXMax;
        }
        
        ScreenYMax = bottomY;
        if (bottomY > ClipYMax) {
            ScreenYMax = ClipYMax;
        }
        
        S32 width = ScreenXMax - ScreenXMin;
        S32 height = ScreenYMax - ScreenYMin;
        
        if (width <= 0 || height <= 0) return;
        
        U8 *dstPtr = (U8 *)Log + TabOffLine[ScreenYMin] + ScreenXMin;
        U8 *srcPtr = spritePtr + sizeof(SpriteStruct) + startY * sprite->deltaX + startX;
        U8 *transpTable = (U8 *)ptr_transp;
        
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
    S32 newDeltaX = (sprite->deltaX * factorx) >> 16;
    S32 newDeltaY = (sprite->deltaY * factory) >> 16;
    S32 newHotX = ((S8)sprite->hotX * factorx) >> 16;
    S32 newHotY = ((S8)sprite->hotY * factory) >> 16;
    
    S32 leftX = x + newHotX;
    S32 topY = y + newHotY;
    S32 rightX = leftX + newDeltaX;
    S32 bottomY = topY + newDeltaY;
    
    if (leftX > ClipXMax || topY > ClipYMax || rightX < ClipXMin || bottomY < ClipYMin) {
        ScreenXMin = 32000;
        ScreenXMax = -32000;
        ScreenYMin = 32000;
        ScreenYMax = -32000;
        return;
    }
    
    S32 invFactorX = (1 << 16) / factorx;
    S32 invFactorY = (1 << 16) / factory;
    
    S32 startTexX = ((sprite->deltaX >> 1) << 16) - (invFactorX * (newDeltaX >> 1));
    S32 startTexY = ((sprite->deltaY >> 1) << 16) - (invFactorY * (newDeltaY >> 1));
    
    ScreenXMin = leftX;
    ScreenYMin = topY;
    
    if (leftX < ClipXMin) {
        S32 clipDelta = ClipXMin - leftX;
        startTexX += clipDelta * invFactorX;
        ScreenXMin = ClipXMin;
    }
    
    if (topY < ClipYMin) {
        S32 clipDelta = ClipYMin - topY;
        startTexY += clipDelta * invFactorY;
        ScreenYMin = ClipYMin;
    }
    
    ScreenXMax = rightX;
    if (rightX > ClipXMax) {
        ScreenXMax = ClipXMax;
    }
    
    ScreenYMax = bottomY;
    if (bottomY > ClipYMax) {
        ScreenYMax = ClipYMax;
    }
    
    S32 width = ScreenXMax - ScreenXMin;
    S32 height = ScreenYMax - ScreenYMin;
    
    if (width <= 0 || height <= 0) return;
    
    U8 *dstPtr = (U8 *)Log + TabOffLine[ScreenYMin] + ScreenXMin;
    U8 *srcBase = spritePtr + sizeof(SpriteStruct);
    U8 *transpTable = (U8 *)ptr_transp;
    
    S32 texY = startTexY;
    
    for (S32 scanY = 0; scanY < height; scanY++) {
        S32 texX = startTexX;
        U8 *dstLine = dstPtr;
        
        S32 srcY = texY >> 16;
        if (srcY >= 0 && srcY < sprite->deltaY) {
            U8 *srcLine = srcBase + srcY * sprite->deltaX;
            
            for (S32 scanX = 0; scanX < width; scanX++) {
                S32 srcX = texX >> 16;
                if (srcX >= 0 && srcX < sprite->deltaX) {
                    U8 srcPixel = srcLine[srcX];
                    if (srcPixel != 0) {
                        U8 dstPixel = *dstLine;
                        U16 blendIndex = (srcPixel << 8) | dstPixel;
                        *dstLine = transpTable[blendIndex];
                    }
                }
                dstLine++;
                texX += invFactorX;
            }
        }
        
        dstPtr += ModeDesiredX;
        texY += invFactorY;
    }
}

#ifdef __cplusplus
}
#endif
