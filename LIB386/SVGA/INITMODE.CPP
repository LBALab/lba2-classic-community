#include <SVGA/INITMODE.H>

#include <SVGA/CLIP.H>
#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/LIMITS.H>
#include <SYSTEM/LOGPRINT.H>
#include <SYSTEM/WINDOW.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Initialization ----------------------------------------------------------
bool InitGraphics(U32 resX, U32 resY) {
  if (resY > ADELINE_MAX_Y_RES) {
    return false;
  }

  LogPrintf("\nDesired resolution: %i*%i\n", resX, resY);

  if (!CreateWindowSurface(resX, resY)) {
    return false;
  }

  if (!CreateVideoSurface(resX, resY)) {
    return false;
  }

  if (!CreateScreenMemory(resX, resY)) {
    return false;
  }

  SetClipWindow(0, 0, resX - 1, resY - 1);
  SetClip(0, 0, resX - 1, resY - 1);

  return true;
}

void EndGraphics() {
  // Empty
}

// =============================================================================
#ifdef __cplusplus
}
#endif
