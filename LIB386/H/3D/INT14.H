#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

/* The 1997 integer path's 2.14 fixed point back to integers, as its ASM rounds
 * it (IMATSTDI, LROT3DI, LIROT3DI). Shared so the three ports round the same way.
 *
 * Right shifts of negative values are arithmetic on every compiler lba2cc
 * builds with; the casts through U32/U64 keep the 32-bit wrap defined. */

/* A 32-bit 2.14 product back to 2.14, rounded half up: the ASM's sar 14 then
   adc 0, or lea +(1 SHL 13) then sar 14, which give the same value. */
static inline S32 Int14_Round(S32 v) {
    return (v + (1 << 13)) >> 14;
}

/* A 64-bit sum of 2.14 products back to an integer: the ASM's shrd 14 then
   adc 0, which adds the last bit shifted out (bit 13) and keeps the low 32 bits. */
static inline S32 Int14_Shr14Round(S64 v) {
    return (S32)(U32)((U64)(v >> 14) + (U64)((v >> 13) & 1));
}
