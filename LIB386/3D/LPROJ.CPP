#include <3D/LPROJ.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#undef LongProjectPoint
extern Func_LongProjectPoint *LongProjectPoint;

S32 LongProjectPoint_C(S32 x, S32 y, S32 z) {
  S32 result;

#ifdef __MSC_VER
  __asm {
		pusha
		mov eax, x
		mov ebx, y
		mov ecx, z
		call LongProjectPoint
		mov result, eax
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %1, %%eax\n\t"
                       "movl %2, %%ebx\n\t"
                       "movl %3, %%ecx\n\t"
                       "call *%4\n\t"
                       "movl %%eax, %0\n\t"
                       "popa\n\t"
                       : "=m"(result)
                       : "m"(x), "m"(y), "m"(z), "m"(LongProjectPoint)
                       : "%eax", "%ebx", "%ecx");
#else
#error "Unsupported compiler!"
#endif

  return result;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
