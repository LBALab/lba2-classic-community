#include <3D/IMATSTD.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#undef InitMatrixStd
extern Func_InitMatrix *InitMatrixStd;

void InitMatrixStd_C(TYPE_MAT *MatDst, S32 alpha, S32 beta, S32 gamma) {
  /*
#ifdef __MSC_VER
  __asm {
		pusha
		mov edi, MatDst
		mov eax, alpha
		mov ebx, beta
		mov ecx, gamma
		call InitMatrixStd
		popa
  }
#elif __GNUC__
  __asm__ __volatile__("pusha\n\t"
                       "movl %0, %%edi\n\t"
                       "movl %1, %%eax\n\t"
                       "movl %2, %%ebx\n\t"
                       "movl %3, %%ecx\n\t"
                       "call *%4\n\t"
                       "popa\n\t"
                       :
                       : "r"(MatDst), "m"(alpha), "m"(beta), "m"(gamma),
                         "m"(InitMatrixStd)
                       : "%edi", "%eax", "%ebx", "%ecx");
#else
#error "Unsupported compiler!"
#endif
*/
}

// =============================================================================
#ifdef __cplusplus
}
#endif
