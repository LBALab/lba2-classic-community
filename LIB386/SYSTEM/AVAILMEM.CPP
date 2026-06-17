#include <SYSTEM/AVAILMEM.H>

#include <SDL3/SDL_cpuinfo.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Interface ---------------------------------------------------------------
U64 AvailableMem() {
    // SDL_GetSystemRAM() reports installed RAM in MiB; the engine's memory
    // budgeting (AdjustHQRMem) works in bytes, so convert. Returning MiB here
    // silently collapsed every HQR cache to its minimum, because the
    // byte-fraction formulas saw a tiny number (e.g. 32768 for 32 GB).
    return (U64)SDL_GetSystemRAM() * 1024 * 1024;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
