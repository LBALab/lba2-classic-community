#include <SVGA/BLITBOX.H>

#include <SVGA/VIDEO.H>
#include <SYSTEM/DEBUG_TRACES.H>

#include <string.h>

void BlitBoxF(void *dst, void *src) {
    U8 *dst_bytes = (U8 *)dst;
    U8 *src_bytes = (U8 *)src;
    U8 *dst_line = dst_bytes + TabOffPhysLine[140] + 160;
    U32 pitch = TabOffPhysLine[1];

    LIB386_TRACE_CPP("BlitBoxF", "1", "dst=%p src=%p", dst, src);

    for (U32 y = 0; y < 200; ++y) {
        for (U32 x = 0; x < 320; x += 16) {
            double chunk0_bits;
            double chunk1_bits;
            volatile double chunk0_fp;
            volatile double chunk1_fp;
            double chunk0_out;
            double chunk1_out;

            memcpy(&chunk0_bits, src_bytes + x, 8);
            memcpy(&chunk1_bits, src_bytes + x + 8, 8);

            chunk0_fp = chunk0_bits;
            chunk1_fp = chunk1_bits;

            chunk0_out = chunk0_fp;
            chunk1_out = chunk1_fp;

            memcpy(dst_line + x, &chunk0_out, 8);
            memcpy(dst_line + x + 8, &chunk1_out, 8);
        }
        dst_line += pitch;
        src_bytes += 320;
    }
}

// -----------------------------------------------------------------------------
Func_BlitBox *BlitBox = BlitBoxF;
