/*
    DWORD	TabOffset[]
      Brick:
          - BYTE Delta X
          - BYTE Delta Y
        Line(Delta Y):
        - BYTE NbBlock
           Block 0:	Nb Zero to Jump
           Block 1:	Nb Zero to Write
           Block 2:	Nb Zero to Jump
          etc...
*/

#include <SVGA/COPYMASK.H>

#include <string.h>

#include <SYSTEM/ADELINE.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SYSTEM/DEBUG_TRACES.H>

typedef struct {
    U8 DeltaX;
    U8 DeltaY;
    U8 HotX;
    U8 HotY;
} Struc_Mask_Header;

void CopyMask(S32 nummask, S32 x, S32 y, U8 *bankmask, void *src) {
    LIB386_TRACE_CPP("CopyMask", "1", "num=%d x=%d y=%d bank=%p src=%p", nummask, x, y, (void *)bankmask, (void *)src);
    S32 screenWidth = (S32)TabOffLine[1];

    Struc_Mask_Header *maskHeader = (Struc_Mask_Header *)(bankmask + ((U32 *)bankmask)[nummask]);
    // Geometry must be signed: callers pass x as low as -47 (the DrawOverBrick
    // family re-masks column 0, which holds bricks from XScreen -47). On the
    // original 32-bit build, U32 arithmetic combined with 32-bit pointer
    // wraparound made Log + (U32)-24 land at Log - 24. On 64-bit, that
    // zero-extends to Log + 4 GiB and segfaults — and the U32 clip test
    // (xMin < ClipXMin) silently fails for negative xMin, so margins stay 0.
    S32 xMin = (S32)maskHeader->HotX + x;
    S32 yMin = (S32)maskHeader->HotY + y;
    U8 *maskData = (U8 *)maskHeader + sizeof(Struc_Mask_Header); // Skip header

    // Test Clipping
    S32 xMax = (S32)maskHeader->DeltaX + xMin - 1;
    S32 yMax = (S32)maskHeader->DeltaY + yMin - 1;

    S32 marginTop = 0;
    S32 marginLeft = 0;
    S32 marginRight = 0;
    S32 marginBottom = 0;

    if (xMin < ClipXMin || yMin < ClipYMin || xMax > ClipXMax || yMax > ClipYMax) {
        // ClippingMask:
        if (xMin > ClipXMax || yMin > ClipYMax || xMax < ClipXMin || yMax < ClipYMin) {
            return;
        }

        if (yMin < ClipYMin) {
            // Clipping top
            marginTop = ClipYMin - yMin;
        }
        // Clipping bottom
        // PasHaut:
        if (yMax > ClipYMax) {
            marginBottom = yMax - ClipYMax;
        }
        // Clipping left
        // PasBas:
        if (xMin < ClipXMin) {
            marginLeft = ClipXMin - xMin;
        }
        // Clipping right
        // PasGauche:
        if (xMax > ClipXMax) {
            marginRight = xMax - ClipXMax;
        }
    }

    // Calculate Offset Screen
    // A mask can start above the screen: a brick over a door near the top
    // edge has yMin < 0. Like the original, the rows above ClipYMin are
    // walked in the mask data but not written (marginTop). Offsets are
    // signed so no pointer is formed outside the buffers.
    S32 deltaX = xMax - xMin + 1; // (deltaX)
    S32 offset = yMin * screenWidth + xMin;
    U8 *screen = (U8 *)Log;
    const U8 *source = (const U8 *)src;
    S32 deltaY = yMax - yMin + 1; // NbLine (deltaY)
    S32 lineOffset = screenWidth - deltaX;
    for (S32 y = 0; y < deltaY; y++) {
        // NextLine:
        U8 numberOfBlocks = *maskData; // Nb Block for this line
        maskData++;
        S32 x = 0;

        do {
            // SameLine:
            S32 numberOfZeroToJump = *maskData; // Nb Zero to Jump
            maskData++;
            offset += numberOfZeroToJump; // Incrust on Log and on PtSrc
            numberOfBlocks--;
            if (numberOfBlocks == 0) {
                break;
            }

            x += numberOfZeroToJump;

            S32 numberOfPixelToCopy = *maskData; // Nb Zero to Write
            maskData++;
            do {
                // loopb:
                if (x >= marginLeft && x < deltaX - marginRight && y >= marginTop && y < deltaY - marginBottom) {
                    screen[offset] = source[offset];
                }
                x++;
                offset++;
                --numberOfPixelToCopy;
            } while (numberOfPixelToCopy);
            numberOfBlocks--;
        } while (numberOfBlocks);

        offset += lineOffset;
    }
}
