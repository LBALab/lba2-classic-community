#include <POLYGON/POLY.H>

#include <limits.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
S32 INV64(S32 a) { return LONG_MIN / a; }

#undef Fill_Poly
S32 Fill_Poly_C(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
                Struc_Point *Ptr_Points) {
  S32 returnValue;

#ifdef __MSC_VER
  __asm {
		pusha
		mov eax, Type_Poly
		mov ebx, Color_Poly
		mov ecx, Nb_Points
		mov esi, Ptr_Points
		call Fill_PolyMMX
		mov returnValue, eax
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "mov %1, %%eax\n\t"
                       "mov %2, %%ebx\n\t"
                       "mov %3, %%ecx\n\t"
                       "mov %4, %%esi\n\t"
                       "call %5\n\t"
                       "mov %%eax, %0\n\t"
                       "popa\n\t"
                       : "=m"(returnValue)
                       : "m"(Type_Poly), "m"(Color_Poly), "m"(Nb_Points),
                         "m"(Ptr_Points), "m"(Fill_PolyFast)
                       : "%eax", "%ebx", "%ecx", "%esi");
#else
#error "Unsupported compiler!"
#endif

  return returnValue;
}

void Switch_Fillers_C(U32 Bank) {
#ifdef __MSC_VER
  __asm {
		pusha
		mov eax, Bank
		call Switch_Fillers_ASM
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %0, %%eax\n\t"
                       "call %1\n\t"
                       "popa\n\t"
                       :
                       : "m"(Bank), "m"(Switch_Fillers_ASM)
                       : "%eax");
#else
#error "Unsupported compiler!"
#endif
}

// =============================================================================
#ifdef __cplusplus
}
#endif
