#include <SYSTEM.H>

S32 MULDIV64(S32 x, S32 mul, S32 div) // (x*mul)/div
{
  S32 result;

#ifdef __MSC_VER
  __asm {
		pusha
		mov eax, x
		mov edx, mul
		mov ecx, div
		imul edx
		idiv ecx
		mov result, eax
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %1, %%eax\n\t"
                       "movl %2, %%edx\n\t"
                       "movl %3, %%ecx\n\t"
                       "imul %%edx\n\t"
                       "idiv %%ecx\n\t"
                       "movl %%eax, %0\n\t"
                       "popa\n\t"
                       : "=r"(result)
                       : "r"(x), "r"(mul), "r"(div)
                       : "%eax", "%ecx", "%edx");
#else
#error "Unsupported compiler!"
#endif

  return result;
}
