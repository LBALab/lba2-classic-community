/* Host test for INV64 (LIB386/pol_work/POLY.CPP).
 *
 * The 1997 code is an inline pragma in POLY.H, the same sequence the DOS
 * executable runs in the sky and sea Draw_Poly:
 *
 *     mov  eax, 80000000h
 *     xor  edx, edx
 *     idiv ebx
 *
 * EDX:EAX is 2^31, so the result is 2^31 / a, truncated toward zero
 * (NB_BITS_INV64 is 31). The first C version returned 2^32 / a.
 */

#include <POLYGON/POLY.H>

#include <cstdio>

static int failures = 0;

/* What idiv computes for EDX:EAX = 2^31. */
static S32 Reference(S32 a) {
    return (S32)(0x80000000LL / a);
}

static void Check(S32 a) {
    const S32 got = INV64(a);
    const S32 want = Reference(a);
    if (got != want) {
        std::printf("FAIL INV64(%d) = %d, want %d\n", (int)a, (int)got, (int)want);
        failures++;
    }
}

int main() {
    /* Depths Draw_Poly passes: NearClip + 1 up to the far fog plane, and a few
       negative values for the sign. */
    const S32 fixed[] = {2, 3, 7, 129, 130, 256, 1000, 3001, 6860, 32767, 32768, 65535, 60000 * 1024, -2, -3, -129, -32768};
    for (unsigned i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++)
        Check(fixed[i]);
    for (S32 a = 2; a < 200000; a += 37)
        Check(a);

    /* A sea corner at depth 6860 (Volcano Island): 313044. The old code gave 626088. */
    if (INV64(6860) != 313044) {
        std::printf("FAIL INV64(6860) = %d, want 313044\n", (int)INV64(6860));
        failures++;
    }

    if (failures)
        return 1;
    std::printf("PASS test_inv64\n");
    return 0;
}
