#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

#include <string.h>

/* Little-endian data access.

   Game data on disk (HQR entries, scenes, scripts, saves) is little-endian and
   packed: a 16- or 32-bit field can sit at any byte offset. Read and write such
   fields through these functions, never through a cast pointer such as
   *(S16 *)ptr. A cast read is undefined at a misaligned address, which 32-bit ARM
   enforces (a paired load faults with SIGBUS), and it gives byte-swapped values on
   a big-endian CPU. These build the value from single bytes, so they are defined
   at any address and give the same value on any CPU. GCC and Clang merge the
   bytes back into one load on x86 and arm64, so they cost nothing there. */

static inline U16 ReadLE_U16(const void *ptr) {
    const U8 *b = (const U8 *)ptr;
    return (U16)(b[0] | (b[1] << 8));
}

static inline S16 ReadLE_S16(const void *ptr) {
    return (S16)ReadLE_U16(ptr);
}

static inline U32 ReadLE_U32(const void *ptr) {
    const U8 *b = (const U8 *)ptr;
    return (U32)b[0] | ((U32)b[1] << 8) | ((U32)b[2] << 16) | ((U32)b[3] << 24);
}

static inline S32 ReadLE_S32(const void *ptr) {
    return (S32)ReadLE_U32(ptr);
}

static inline void WriteLE_U16(void *ptr, U16 value) {
    U8 *b = (U8 *)ptr;
    b[0] = (U8)value;
    b[1] = (U8)(value >> 8);
}

static inline void WriteLE_S16(void *ptr, S16 value) {
    WriteLE_U16(ptr, (U16)value);
}

static inline void WriteLE_U32(void *ptr, U32 value) {
    U8 *b = (U8 *)ptr;
    b[0] = (U8)value;
    b[1] = (U8)(value >> 8);
    b[2] = (U8)(value >> 16);
    b[3] = (U8)(value >> 24);
}

static inline void WriteLE_S32(void *ptr, S32 value) {
    WriteLE_U32(ptr, (U32)value);
}

/* Converting a whole resource once, at load.

   The renderer and the animation code read bodies, animations and similar
   tables through typed pointers in their inner loops. Rather than change every
   such read, a loader converts the resource in place right after loading it,
   from little-endian to the CPU's order. On a little-endian CPU there is
   nothing to do, and these compile to nothing. */

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define LBA_BIG_ENDIAN 1
#else
#define LBA_BIG_ENDIAN 0
#endif

static inline void FromLE16InPlace(void *ptr, U32 count) {
    if (LBA_BIG_ENDIAN) {
        U8 *b = (U8 *)ptr;
        for (U32 i = 0; i < count; i++, b += 2) {
            const U16 v = ReadLE_U16(b);
            memcpy(b, &v, 2);
        }
    }
}

static inline void FromLE32InPlace(void *ptr, U32 count) {
    if (LBA_BIG_ENDIAN) {
        U8 *b = (U8 *)ptr;
        for (U32 i = 0; i < count; i++, b += 4) {
            const U32 v = ReadLE_U32(b);
            memcpy(b, &v, 4);
        }
    }
}
