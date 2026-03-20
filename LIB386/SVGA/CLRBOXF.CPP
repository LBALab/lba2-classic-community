#include <SVGA/CLRBOX.H>
#include <stdio.h>
#include <string.h>
#include <SYSTEM/DEBUG_TRACES.H>

// ClearColor	dq	0
U64 ClearColor = 0;

void ClearBox(void *dst, U32 *TabOffDst, T_BOX *box) {
    LIB386_TRACE_CPP("ClearBoxF", "1", "dst=%p tab=%p box=%p", dst, (void *)TabOffDst, (void *)box);
    // The original code used the FPU to clear the screen, by copying the clear
    // color (which is a double -8 bytes- value, with the actual clear color -1 byte-
    // repeated 8 times) to the screen, overriding 8 bytes at a time.
    // Since I don't know if this will be a bottleneck in modern devices, I'll
    // use the simplest and most naive implementation.

    U8 *dst8 = (U8 *)dst;
    U8 color = ClearColor & 0xFF;

    for (S16 y = box->y0; y < box->y1; y++) {
        U8 *start = dst8 + TabOffDst[y] + box->x0;
        memset(start, color, box->x1 - box->x0);
    }
}

void SetClearColor(U32 color) {
    // The clear color is only 1 byte, but it's stored in a 8 byte variable
    // as a more efficient way of clearing the screen, overriding 8 byte at a time
    memset(&ClearColor, color & 0xFF, sizeof(ClearColor));
}
