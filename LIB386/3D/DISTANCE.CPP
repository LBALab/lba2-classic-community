#include <3D/DISTANCE.H>

#include <3D/SQRROOT.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// The same arithmetic as the 1997 DISTANCE.ASM: the deltas wrap in 32 bits
// (`sub`), each square is a full 64-bit product (one-operand `imul`), the sum
// is 64-bit (`add`/`adc`) and QSqr takes the integer square root, rounded down.
// Squaring in 32 bits overflowed past 46340 in 2D and 26754 per axis in 3D.
static S32 WrapSub(S32 a, S32 b) {
    return (S32)((U32)a - (U32)b);
}

static U64 Square(S32 d) {
    return (U64)((S64)d * d);
}

static U32 Root(U64 sum) {
    return QSqr((U32)sum, (U32)(sum >> 32));
}

U32 Distance2D(S32 x0, S32 y0, S32 x1, S32 y1) {
    return Root(Square(WrapSub(x0, x1)) + Square(WrapSub(y0, y1)));
}

U32 Distance3D(S32 x0, S32 y0, S32 z0, S32 x1, S32 y1, S32 z1) {
    return Root(Square(WrapSub(x0, x1)) + Square(WrapSub(y0, y1)) + Square(WrapSub(z0, z1)));
}

// =============================================================================
#ifdef __cplusplus
}
#endif
