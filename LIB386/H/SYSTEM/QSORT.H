//──────────────────────────────────────────────────────────────────────────
#ifndef LIB_SYSTEM_QSORT
#define LIB_SYSTEM_QSORT

#include <stddef.h>

//──────────────────────────────────────────────────────────────────────────
// The qsort the 1997 game was linked with, for sorts whose ties show.
//
// The C library's qsort only promises a sorted result. Where two elements
// compare equal, which one comes first is the library's own business: glibc,
// macOS and Windows each order them differently, and so did the Watcom library
// the game shipped with. A draw-order sort makes that visible, since whichever
// of two objects at the same depth is drawn last covers the other.
//
// RetailQsort is that Watcom qsort, ported from the 1997 DOS build (LBA2.DOS,
// 0x63ed6): a Bentley-McIlroy three-way quicksort, with an insertion pass at
// gaps of three elements then one below 16 elements, the median of three as the
// pivot above 29 and the median of three medians above 42, and an explicit stack
// in place of recursion. Its order for equal elements is the original's on every
// platform. tests/qsort holds orders taken by running the original code.

#ifdef __cplusplus
extern "C" {
#endif

void RetailQsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));

#ifdef __cplusplus
}
#endif

#endif // LIB_SYSTEM_QSORT
