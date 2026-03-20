#include <SVGA/MASK.H>
#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/SCREENXY.H>
#include <stdio.h>
#include <SYSTEM/DEBUG_TRACES.H>

U8 ColMask = 0;

S32 ClippingMask(S32 x0, S32 y0, S32 x1, S32 y1, U8 *graphics) {
    LIB386_TRACE_CPP("ClippingMask", "1", "num=%d x0=%d y0=%d x1=%d y1=%d gfx=%p", x0, y0, x1, y1, (void *)graphics);
    if (x0 > ClipXMax || y0 > ClipYMax ||
        x1 < ClipXMin || y1 < ClipYMin) {
        return 0;
    }

    if (y0 < ClipYMin) {
        // ---------------------- Clipping Top, Skip ClipYMin-ECX Line(s)

        S32 yDiff = ClipYMin - y0;
        for (int i = 0; i < yDiff; i++) {
            // NextH:
            U8 nbBlock = graphics[0];
            graphics += nbBlock + 1; // Jump data
        }

        y0 = ClipYMin;
    }

    // ;---------------------- Clipping Bottom
    // PasHaut:
    if (y1 > ClipYMax) {
        y1 = ClipYMax;
    }

    // ;---------------------- Clipping Left
    // PasBas:
    U32 OffsetBegin = 0;
    if (x0 < ClipXMin) {
        OffsetBegin = ClipXMin - x0;
    }

    // PasGauche:
    // ;---------------------- Clipping Right
    S32 offsetEnd = x1 - x0 - OffsetBegin + 1;

    if (x1 > ClipXMax) {
        x1 -= ClipXMax;
        offsetEnd -= x1;
        x1 = ClipXMax;
    }

    // PasDroit:
    ScreenXMax = x1;
    x1 = OffsetBegin;

    x1 += x0;
    ScreenYMin = y0;
    ScreenXMin = x1;
    ScreenYMax = y1;

    U8 *screen = (U8 *)Log + TabOffLine[y0] + x0 + OffsetBegin;

    S32 deltaY = y1 - y0;
    for (int i = 0; i <= deltaY; i++) {
        U8 color = ColMask;
        U8 nbBlock = graphics[0];
        graphics++;
        U32 bytesToSkip = OffsetBegin;
        S32 offset = offsetEnd;

        while (nbBlock > 0) {
            // Clipper:
            U8 nbTransp = graphics[0];
            graphics++;
            nbBlock--;
            if (nbBlock == 0) {
                break;
            }

            bytesToSkip -= nbTransp;
            if (bytesToSkip < 0) {
                // Start_Transp:
                offset += bytesToSkip;
                if (offset <= 0) {
                    break;
                }
                screen -= bytesToSkip;
                // Do_Fill:
                U8 counter = graphics[0];
                graphics++;
                if (offset < nbTransp) {
                    nbTransp = offset;
                }
                offset -= nbTransp;

                // Do_Fill_1:
                for (int j = 0; j < nbTransp; j++) {
                    *screen = color;
                    screen++;
                }

                nbBlock--;
                if (nbBlock == 0) {
                    break;
                }
            }

            U8 nbFill = graphics[0];
            graphics++;
            bytesToSkip -= nbFill;
            if (bytesToSkip < 0) {
                // Start_Fill:
                bytesToSkip = -bytesToSkip;
                if (offset < bytesToSkip) {
                    bytesToSkip = offset;
                }
                offset -= bytesToSkip;
                nbFill = bytesToSkip;
                // Do_Fill_1:
                for (int j = 0; j < nbFill; j++) {
                    *screen = color;
                    screen++;
                }

                nbBlock--;
                if (nbBlock == 0) {
                    break;
                }
            }

            nbBlock--;
            if (nbBlock == 0) {
                break;
            }
        }

        screen += TabOffLine[1];
        graphics += nbBlock;
    }

    return 0; // TODO: Remove, make void
}

S32 AffMask(S32 nummask, S32 x, S32 y, void *bankmask) {
    U8 *data = (U8 *)bankmask + ((U32 *)bankmask)[nummask];
    U8 deltaX = data[0];
    U8 nbLine = data[1]; // delta Y
    S32 hotX = data[2];
    S32 hotY = data[3];

    U8 *graphics = data + 4;

    x += hotX;
    y += hotY;

    S32 x1 = x + deltaX - 1;
    S32 y1 = y + nbLine - 1;

    // ----------------------------------------------- Test Clipping
    if (x < ClipXMin || y < ClipYMin || x1 > ClipXMax || y1 > ClipYMax) {
        return ClippingMask(x, y, x1, y1, graphics);
    }

    ScreenXMin = x;
    ScreenXMax = x1;
    ScreenYMin = y;
    ScreenYMax = y1;

    // ----------------------------------------------- Compute Offset Ecran
    U8 *screen = (U8 *)Log + TabOffLine[y] + x;
    U32 offsetScreen = TabOffLine[1] - deltaX;
    U8 color = ColMask;

    for (int i = 0; i < nbLine; i++) {
        // ;-------------------- Init NbBlock for this line
        // NextLine:
        U8 nbBlock = graphics[0];
        graphics++;

        while (nbBlock > 0) {
            // ;-------------------- Manage One Line
            // SameLine:
            U8 toJump = graphics[0]; // Nb Zero to jump
            screen += toJump;
            nbBlock--;
            if (nbBlock == 0) {
                // EndBlock:
                graphics++;
                break;
            }

            U8 toWrite = graphics[1]; // Nb Zero to write
            graphics += 2;
            for (int k = 0; k < toWrite; k++) {
                *(screen++) = color;
            }

            nbBlock--;
        }
        screen += offsetScreen;
    }

    return 0; // TODO: make it void?
}
