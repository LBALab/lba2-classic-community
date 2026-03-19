#include <3D/SQRROOT.H>

#include <SYSTEM/DEBUG_TRACES.H>

static U32 highest_bit_index(U32 value) {
    U32 index = 0;
    while (value >>= 1) {
        ++index;
    }
    return index;
}

static U32 shld32(U32 dest, U32 src, U32 count) {
    if (count == 0) {
        return dest;
    }
    return (U32)(((U64)dest << count) | ((U64)src >> (32 - count)));
}

static U32 sqr32_asm_exact(U32 value) {
    U32 eax = value;
    U32 ebx;
    U32 ecx;
    U32 edx;
    U32 esi;

    if (eax <= 3) {
        return eax == 0 ? 0 : 1;
    }

    ecx = (33u - highest_bit_index(eax)) & ~1u;
    ebx = eax;
    edx = shld32(0, ebx, ecx);
    ebx <<= ecx;
    ecx = highest_bit_index(eax);
    eax = 1;
    --edx;
    ecx >>= 1;

    for (;;) {
        U64 sum;
        U32 carry;
        U32 previous_edx;

        sum = (U64)ebx + ebx;
        ebx = (U32)sum;
        carry = (U32)(sum >> 32);

        esi = eax * 4u + 1u;

        sum = (U64)edx + edx + carry;
        edx = (U32)sum;
        carry = (U32)(sum >> 32);

        sum = (U64)ebx + ebx;
        ebx = (U32)sum;
        carry = (U32)(sum >> 32);

        sum = (U64)edx + edx + carry;
        edx = (U32)sum;

        eax = eax * 2u + 1u;
        previous_edx = edx;
        edx -= esi;
        if (previous_edx >= esi) {
            --ecx;
            if (ecx == 0) {
                return eax;
            }
            continue;
        }

        edx += esi;
        --eax;
        --ecx;
        if (ecx == 0) {
            return eax;
        }
    }
}

static U32 sqr64_asm_exact(U32 xLow, U32 xHigh) {
    U32 eax = xLow;
    U32 ebx;
    U32 ecx;
    U32 edx = xHigh;
    U32 edi;
    U32 esi;

    if (edx == 0) {
        return sqr32_asm_exact(eax);
    }

    if (edx <= 3) {
        edi = edx - 1u;
        edx = eax;
        ebx = 0;
        ecx = 16;
        eax = 1;
    } else {
        U32 shift_count;
        U32 bit_index;

        ebx = eax;
        bit_index = highest_bit_index(edx);
        shift_count = (33u - bit_index) & ~1u;
        eax = bit_index + 32u;
        edi = shld32(0, edx, shift_count);
        edx = shld32(edx, ebx, shift_count);
        ebx <<= shift_count;
        ecx = eax >> 1;
        eax = 1;
        --edi;
    }

    for (;;) {
        U64 sum;
        U32 carry;
        U32 previous_edi;

        sum = (U64)ebx + ebx;
        ebx = (U32)sum;
        carry = (U32)(sum >> 32);

        esi = eax * 4u + 1u;

        sum = (U64)edx + edx + carry;
        edx = (U32)sum;
        carry = (U32)(sum >> 32);

        eax = eax * 2u + 1u;

        sum = (U64)edi + edi + carry;
        edi = (U32)sum;
        carry = (U32)(sum >> 32);

        sum = (U64)ebx + ebx;
        ebx = (U32)sum;
        carry = (U32)(sum >> 32);

        sum = (U64)edx + edx + carry;
        edx = (U32)sum;
        carry = (U32)(sum >> 32);

        sum = (U64)edi + edi + carry;
        edi = (U32)sum;

        previous_edi = edi;
        edi -= esi;
        if (previous_edi >= esi) {
            --ecx;
            if (ecx == 0) {
                return eax;
            }
            continue;
        }

        edi += esi;
        --eax;
        --ecx;
        if (ecx == 0) {
            return eax;
        }
    }
}

U32 Sqr(U32 x) {
    LIB386_TRACE_CPP("Sqr", "1", "x=%u", x);
    U32 result = sqr32_asm_exact(x);
    LIB386_TRACE_CPP("Sqr", "2", "return=%u", result);
    return result;
}

U32 QSqr(U32 xLow, U32 xHigh) {
    LIB386_TRACE_CPP("QSqr", "1", "xLow=%u xHigh=%u", xLow, xHigh);
    U32 result = sqr64_asm_exact(xLow, xHigh);
    LIB386_TRACE_CPP("QSqr", "2", "return=%u", result);
    return result;
}
