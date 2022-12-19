#include <SYSTEM/ADELINE.H>

#ifdef __cplusplus
extern "C" {
#endif

int _VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect,
                    PDWORD lpflOldProtect, DWORD dummy) {
  DWORD oldProtect = 0;
  VirtualProtect(lpAddress, dwSize, flNewProtect, &oldProtect);

  return 0;
}

#ifdef __cplusplus
}
#endif
