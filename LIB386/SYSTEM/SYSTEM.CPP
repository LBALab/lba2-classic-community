#include <SYSTEM.H>

#include <assert.h>

// (x * mul) / div with a 64-bit product, as the original `imul; idiv` did.
// The terrain slide in DoCornerReajustExtPt passes mul = sin * 65536 (up to
// 2^30), so a 32-bit product overflows for any step of 2 units or more.
// The product of two S32 always fits in S64; the quotient must fit in S32,
// where `idiv` would have faulted.
S32 MULDIV64(S32 x, S32 mul, S32 div) {
    assert(div != 0);
    const S64 q = ((S64)x * mul) / div;
    assert(q >= -(S64)0x80000000 && q <= (S64)0x7FFFFFFF);
    return (S32)q;
}
