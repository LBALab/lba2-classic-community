#include <SYSTEM/MOUSE.H>

#include <SVGA/SCREEN.H> // For ModeDesiredX, ModeDesiredY

#include <SDL2/SDL_events.h>
#include <assert.h>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
bool FlagMouse = false;
volatile S32 Click = 0;
volatile U32 MouseX = 0;
volatile U32 MouseY = 0;
S32 MouseXDep = 0;
S32 MouseYDep = 0;

S32 MouseSpriteGraphicNum = 0;
const void *MouseSpriteGraphic = NULL;

// --- Private state -----------------------------------------------------------
S32 LibMouseXDep, LibMouseYDep;
U32 MouseBoxX0, MouseBoxX1, MouseBoxY0, MouseBoxY1;

// --- Extern state ------------------------------------------------------------
// TODO: Convert the asm to .h to hold the data (currently on MOUSEDAT.ASM)
extern U8 BinGphMouse;

// --- Initialization ----------------------------------------------------------
void InitMouse() {
  ClearMouseBox();

  MouseSpriteGraphic = &BinGphMouse;
  MouseSpriteGraphicNum = 0;
}

void EndMouse() {
  // Empty
}

// --- Interface ---------------------------------------------------------------
void ShowMouse(bool show) { FlagMouse = show; }

void SetMousePos(U32 x, U32 y) {
  MouseX = x;
  MouseY = y;

  // Clamp to limits, X
  if (MouseX < MouseBoxX0) {
    MouseX = MouseBoxX0;
  } else if (MouseX > MouseBoxX1) {
    MouseX = MouseBoxX1;
  }

  // Clamp to limits, Y
  if (MouseY < MouseBoxY0) {
    MouseY = MouseBoxY0;
  } else if (MouseY > MouseBoxY1) {
    MouseY = MouseBoxY1;
  }
}

void SetMouseBox(U32 x0, U32 y0, U32 x1, U32 y1) {
  assert((x0 <= x1) && (y0 <= y1));

  // Limits can't be greater than screen
  if (x1 >= ModeDesiredX) {
    x1 = ModeDesiredX - 1;
  }
  if (y1 >= ModeDesiredY) {
    y1 = ModeDesiredY - 1;
  }

  MouseBoxX0 = x0;
  MouseBoxY0 = y0;
  MouseBoxX1 = x1;
  MouseBoxY1 = y1;

  SetMousePos(MouseX, MouseY);
}

void ClearMouseBox() { SetMouseBox(0, 0, ModeDesiredX - 1, ModeDesiredY - 1); }

void ManageMouse() {
  MouseXDep = LibMouseXDep;
  MouseYDep = LibMouseYDep;

  LibMouseXDep = 0;
  LibMouseYDep = 0;
}

void HandleEventsMouse(const void *event) {
  assert(event != NULL);

  const SDL_Event *sdlEvent = (SDL_Event *)(event);

  switch (sdlEvent->type) {
  case SDL_MOUSEMOTION:
    LibMouseXDep += sdlEvent->motion.xrel;
    LibMouseYDep += sdlEvent->motion.yrel;
    SetMousePos(sdlEvent->motion.x, sdlEvent->motion.y);
    break;

  case SDL_MOUSEBUTTONDOWN:
    Click |= (sdlEvent->button.button == 1) ? 1 : 0;
    Click |= (sdlEvent->button.button == 3) ? 2 : 0;
    break;

  case SDL_MOUSEBUTTONUP:
    Click &= (sdlEvent->button.button == 1) ? ~1 : Click;
    Click &= (sdlEvent->button.button == 3) ? ~2 : Click;
    break;
  }
}

// =============================================================================
#ifdef __cplusplus
}
#endif
