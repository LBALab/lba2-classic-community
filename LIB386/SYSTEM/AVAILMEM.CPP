#include <SYSTEM/AVAILMEM.H>

#include <SDL2/SDL_cpuinfo.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Interface ---------------------------------------------------------------
U64 AvailableMem() {
  // Not the same as available, but good enough for now...
  return SDL_GetSystemRAM();
}

// =============================================================================
#ifdef __cplusplus
}
#endif
