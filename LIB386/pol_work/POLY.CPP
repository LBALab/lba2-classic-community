#include <POLYGON/POLY.H>

S32 INV64(S32 a) {
  S32 result;

#ifdef __MSC_VER
  __asm {
		pusha
		mov ebx, a
		mov eax, 80000000h
		xor edx, edx
		idiv ebx
		mov result, eax
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %1, %%ebx\n\t"
                       "movl $0x80000000, %%eax\n\t"
                       "xorl %%edx, %%edx\n\t"
                       "idivl %%ebx\n\t"
                       "movl %%eax, %0\n\t"
                       "popa\n\t"
                       : "=r"(result)
                       : "r"(a)
                       : "%eax", "%ebx", "%edx"
  );
#else
#error "Unsupported compiler!"
#endif
  return result;
}

// S32 Fill_PolyMMX(S32 Type_Poly, S32 Color_Poly, S32 Nb_Points,
//                  Struc_Point *Ptr_Points);

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
                       "call Fill_PolyMMX\n\t"
                       "mov %%eax, %0\n\t"
                       "popa\n\t"
                       : "=m"(returnValue)
                       : "m"(Type_Poly), "m"(Color_Poly), "m"(Nb_Points),
                         "m"(Ptr_Points)
                       : "%eax", "%ebx", "%ecx", "%esi");
#else
#error "Unsupported compiler!"
#endif

  return returnValue;
}

#undef Switch_Fillers
extern void Switch_Fillers(U32 Bank);

void Switch_Fillers_C(U32 Bank) {
#ifdef __MSC_VER
  __asm {
		pusha
		mov eax, Bank
		call Switch_Fillers
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %0, %%eax\n\t"
                       "call Switch_Fillers\n\t"
                       "popa\n\t"
                       :
                       : "m"(Bank)
                       : "%eax");
#else
#error "Unsupported compiler!"
#endif
}
