#ifdef _WIN32 // Portability - Windows and Linux use different methods to configure memory protection
#include <memoryapi.h>
#else // _WIN32
#include <sys/mman.h>
#endif // _WIN32

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#ifdef _WIN32 // Portability - Windows and Linux use different methods to configure memory protection
int _VirtualProtect(LPVOID lpAddress, SIZE_T dwSize) {
  DWORD oldProtect = 0;
  VirtualProtect(lpAddress, dwSize, PAGE_EXECUTE_READWRITE, &oldProtect);

  return 0;
}
#else // _WIN32
int _VirtualProtect(void *addr, size_t len) {
  return mprotect(addr, len, PROT_EXEC | PROT_READ | PROT_WRITE);
}
#endif // _WIN32

// =============================================================================
#ifdef __cplusplus
}
#endif
