#include <SYSTEM/AVAILMEM.H>

#include <PLATFORM/PLATFORM.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Interface ---------------------------------------------------------------
U64 AvailableMem() {
    // g_platform.system.ram_mb() reports installed RAM in MiB; the engine's memory
    // budgeting (AdjustHQRMem) works in bytes, so convert. Returning MiB here
    // silently collapsed every HQR cache to its minimum, because the
    // byte-fraction formulas saw a tiny number (e.g. 32768 for 32 GB).
    return (U64)g_platform.system.ram_mb() * 1024 * 1024;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
