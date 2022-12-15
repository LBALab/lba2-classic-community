#include <3D/LROT3D.H>

#ifdef __cplusplus
extern "C" {
#endif

#undef LongWorldRotatePoint
typedef void(Func_LongRotatePoint)(TYPE_MAT *Mat, S32 x, S32 y, S32 z);

void LongWorldRotatePoint_C(S32 x, S32 y, S32 z) {
#ifdef __MSC_VER
  __asm {
		pusha
		mov esi, offset MatriceWorld
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
                       "call LongRotatePoint\n\t"
                       "popa\n\t"
                       :
                       : "m"(MatriceWorld), "m"(x), "m"(y), "m"(z)
                       : "%esi", "%eax", "%ebx", "%ecx");
#else
#error "Unsupported compiler!"
#endif
}

#ifdef __cplusplus
}
#endif
