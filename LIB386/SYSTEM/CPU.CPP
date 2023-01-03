#include <sysinfoapi.h>
#include <winreg.h>

#ifdef __cplusplus
extern "C" {
#endif

void GetSystemInfo_C(LPSYSTEM_INFO lpSystemInfo) {
  GetSystemInfo(lpSystemInfo);
}

long RegOpenKeyExA_C(HKEY hKey,LPCSTR lpSubKey,DWORD ulOptions,REGSAM samDesired,PHKEY phkResult) {
  return RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

long RegCloseKey_C(HKEY hKey) {
  return RegCloseKey(hKey);
}

#ifdef __cplusplus
}
#endif
