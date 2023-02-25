#include <3D/LROT3D.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void LongRotatePoint_C(TYPE_MAT *Mat, S32 x, S32 y, S32 z) {
/*
#ifdef __MSC_VER
  __asm {
		pusha
		mov esi, MatriceWorld
		mov eax, x
		mov ebx, y
		mov ecx, z
		call LongRotatePoint
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %0, %%esi\n\t"
                       "movl %1, %%eax\n\t"
                       "movl %2, %%ebx\n\t"
                       "movl %3, %%ecx\n\t"
                       "call *%4\n\t"
                       "popa\n\t"
                       :
                       : "r"(Mat), "m"(x), "m"(y), "m"(z),
                         "m"(LongRotatePoint)
                       : "%esi", "%eax", "%ebx", "%ecx");
#else
#error "Unsupported compiler!"
#endif
*/
}

// =============================================================================
#ifdef __cplusplus
}
#endif
