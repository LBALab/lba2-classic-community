#include <SVGA/SCREEN.H>

#include <POLYGON/POLY.H> // For SetScreenPitch()
#include <SYSTEM/A_MALLOC.H>
#include <SYSTEM/LOGPRINT.H>

#include <assert.h>
#include <string.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
void *Log = NULL;
void *Screen = NULL;
U32 ModeDesiredX = 0;
U32 ModeDesiredY = 0;
U32 TabOffLine[ADELINE_MAX_Y_RES];

// --- Initialization ----------------------------------------------------------
bool InitScreen() {
  // Log and Screen are not asserted to be NULL since they may be already
  //  allocated at InitMainBuffer()

  return true;
}

void EndScreen() {
  if (Log) {
    Free(Log);
    Log = NULL;
  }

  if (Screen) {
    Free(Screen);
    Screen = NULL;
  }
}

// --- Interface ---------------------------------------------------------------
bool CreateScreenMemory(U32 resX, U32 resY) {
  // Log and Screen are not asserted to be NULL since they may be already
  //  allocated at InitMainBuffer()

  // Allocate Log
  if (Log == NULL) {
    Log = Malloc(resX * resY + 512);
    if (!Log) {
      LogPuts("Error: Not Enough Memory to allocate Log");
      return false;
    }
  }
  memset(Log, 0, resX * resY + 512);

  // Allocate Screen
  if (Screen == NULL) {
    Screen = Malloc(resX * resY + 512);
    if (!Screen) {
      LogPuts("Error: Not Enough Memory to allocate Screen");
      Free(Log);
      Log = NULL;
      return false;
    }
  }
  memset(Screen, 0, resX * resY + 512);

  // Update public state
  ModeDesiredX = resX;
  ModeDesiredY = resY;

  // Fill the log and screen table
  S32 off = 0;
  for (S32 i = 0; i < ADELINE_MAX_Y_RES; i++) {
    TabOffLine[i] = off;
    off += ModeDesiredX;
  }

  // Adjust memory according to pitch
  {
#ifdef __MSC_VER
    __asm {
      mov ebx, offset TabOffLine
    }
#elif __GNUC__
    __asm__ __volatile__("movl %0, %%ebx\n\t" : : "r"(&TabOffLine) : "%ebx");
#else
#error "Unsupported compiler!"
#endif

    SetScreenPitch(TabOffLine);
  }

  return true;
}

// =============================================================================
#ifdef __cplusplus
}
#endif
