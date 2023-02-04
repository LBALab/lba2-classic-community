#include <SYSTEM/DISPOS.H>

#include <SYSTEM/LOGPRINT.H>

#include <SDL2/SDL_platform.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
#define OS_DOS 0
#define OS_WIN31 1
#define OS_WIN95 2
#define OS_NT 3

// --- Public state ------------------------------------------------------------
U32 OSType = OS_NT; // TODO: Remove, is required by CPU.ASM right now

// --- Interface ---------------------------------------------------------------
void DisplayOS() {
  const char *platformStr = SDL_GetPlatform();
  LogPrintf("Platform: %s\n", platformStr);
}

// =============================================================================
#ifdef __cplusplus
}
#endif
