#include <SYSTEM/CPU.H>

// #include <sysinfoapi.h>
// #include <winreg.h>

#ifdef __cplusplus
extern "C" {
#endif

char	ProcessorManufacturerIDString[]	= "";

s_ProcessorSignature ProcessorSignature;
s_ProcessorFeatureFlags ProcessorFeatureFlags;

TLB	Processor4KBDataTLB		;
TLB	Processor4KBInstructionTLB	;

TLB	Processor4MBDataTLB		;
TLB	Processor4MBInstructionTLB	;

CACHE	ProcessorL1DataCache		;
CACHE	ProcessorL1InstructionCache	;

CACHE	ProcessorL2Cache		;

// void GetSystemInfo_C(LPSYSTEM_INFO lpSystemInfo) {
//   GetSystemInfo(lpSystemInfo);
// }

// long RegOpenKeyExA_C(HKEY hKey,LPCSTR lpSubKey,DWORD ulOptions,REGSAM samDesired,PHKEY phkResult) {
//   return RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
// }

// long RegCloseKey_C(HKEY hKey) {
//   return RegCloseKey(hKey);
// }

#ifdef __cplusplus
}
#endif
