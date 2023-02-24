#include <SYSTEM/N_MALLOC.H>

#include <stdlib.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define MALLOC_MASK (~(MALLOC_ALIGN - 1))

// -----------------------------------------------------------------------------
void *NormMalloc(U32 size) {
  U64 addr, temp;

  temp = (U64)malloc(size + MALLOC_ALIGN);

  if (!temp) {
    return NULL;
  }

  addr = (temp + MALLOC_ALIGN) & MALLOC_MASK;

  *((S8 *)addr - 2) = 0;
  *((S8 *)addr - 1) = (S8)(addr - temp);

  return (void *)addr;
}

void NormFree(void *addr) {
  addr = (void *)((U64)addr - *((S8 *)addr - 1));
  free(addr);
}

void *NormMshrink(void *addr, U32 size) {
  // There is no portable equivalent to '_expand' without changing
  //   code that uses this function, making it fail is valid according to
  //   the "contract" of the function
  return NULL;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
