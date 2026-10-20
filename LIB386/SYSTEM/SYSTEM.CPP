#include <SYSTEM.H>

// (x * mul) / div with a 64-bit product, as the original `imul; idiv` did.
// The terrain slide in DoCornerReajustExtPt passes mul = sin << 16 (up to
// 2^30), so a 32-bit product overflows for any step of 2 units or more.
S32 MULDIV64(S32 x, S32 mul, S32 div) { return (S32)(((S64)x * mul) / div); }
