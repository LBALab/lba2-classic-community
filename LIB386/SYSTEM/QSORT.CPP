#include <SYSTEM/QSORT.H>

#include <string.h>

// Ported from the Watcom C library in the 1997 DOS build (LBA2.DOS): qsort at
// 0x63ed6, its median-of-three helper at 0x63e7e. The order of every comparison
// and swap is the original's, since that order is what decides where equal
// elements end up. See SYSTEM/QSORT.H.

typedef int (*QsortCmp)(const void *, const void *);

static void SwapBytes(char *a, char *b, size_t size) {
    while (size--) {
        const char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static char *Med3(char *a, char *b, char *c, QsortCmp cmp) {
    if (cmp(a, b) > 0) {
        if (cmp(a, c) <= 0)
            return a;
        return cmp(b, c) > 0 ? b : c;
    }
    if (cmp(a, c) >= 0)
        return a;
    return cmp(b, c) > 0 ? c : b;
}

// Each pushed range is at most half of the one it came from, so 32 entries
// cover 2^32 elements; the original keeps the same 32.
#define QSORT_STACK 32

void RetailQsort(void *vbase, size_t n, size_t size, QsortCmp cmp) {
    char *loStack[QSORT_STACK];
    size_t nStack[QSORT_STACK];
    size_t sp = 0;
    char *base = (char *)vbase;
    // A 4-byte element at an aligned address is compared through a copy of the
    // pivot; anything else has its pivot swapped to the front of the range.
    const int pivotCopy = size == 4 && !(((size_t)base | size) & 3);
    char pivot[4];

    for (;;) {
        if (n > 1 && n < 16) {
            char *const end = base + n * size;
            // Gaps of three elements, then one.
            for (size_t g = 3 * size;; g -= 2 * size) {
                for (char *p = base + g; p < end; p += g) {
                    for (char *q = p; q > base; q -= g) {
                        char *const r = q - g;
                        if (cmp(r, q) <= 0)
                            break;
                        SwapBytes(q, r, size);
                    }
                }
                if (g < 2 * size)
                    break;
            }
        } else if (n >= 16) {
            char *m = base + (n >> 1) * size;
            if (n > 29) {
                char *l = base;
                char *h = base + (n - 1) * size;
                if (n > 42) {
                    const size_t d = (n >> 3) * size;
                    l = Med3(base, base + d, base + 2 * d, cmp);
                    m = Med3(m - d, m, m + d, cmp);
                    h = Med3(h - 2 * d, h - d, h, cmp);
                }
                m = Med3(l, m, h, cmp);
            }
            char *pv;
            if (pivotCopy) {
                memcpy(pivot, m, 4);
                pv = pivot;
            } else {
                SwapBytes(base, m, size);
                pv = base;
            }

            // Elements equal to the pivot gather at both ends (a, d) while the
            // rest is partitioned between b and c.
            char *a = base, *b = base;
            char *c = base + (n - 1) * size, *d = c;
            for (;;) {
                int r;
                while (b <= c && (r = cmp(b, pv)) <= 0) {
                    if (r == 0) {
                        SwapBytes(a, b, size);
                        a += size;
                    }
                    b += size;
                }
                while (b <= c && (r = cmp(c, pv)) >= 0) {
                    if (r == 0) {
                        SwapBytes(c, d, size);
                        d -= size;
                    }
                    c -= size;
                }
                if (b > c)
                    break;
                SwapBytes(b, c, size);
                b += size;
                c -= size;
            }

            // Move the equal runs to the middle.
            char *const pn = base + n * size;
            size_t s = (size_t)(a - base) < (size_t)(b - a) ? (size_t)(a - base) : (size_t)(b - a);
            if (s)
                SwapBytes(base, b - s, s);
            s = (size_t)(d - c) < (size_t)(pn - d) - size ? (size_t)(d - c) : (size_t)(pn - d) - size;
            if (s)
                SwapBytes(b, pn - s, s);

            // Keep the smaller side, push the larger.
            const size_t right = (size_t)(d - c), left = (size_t)(b - a);
            if (right >= left) {
                loStack[sp] = pn - right;
                nStack[sp] = right / size;
                sp++;
                n = left / size;
                continue;
            }
            if (left > size) {
                loStack[sp] = base;
                nStack[sp] = left / size;
                sp++;
                base = pn - right;
                n = right / size;
                continue;
            }
        }
        if (sp == 0)
            return;
        sp--;
        base = loStack[sp];
        n = nStack[sp];
    }
}
