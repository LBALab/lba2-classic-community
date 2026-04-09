#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// --- Public state ------------------------------------------------------------
extern bool FlagMouse;      ///< Display mouse if true
extern volatile S32 Click;  ///< Mouse button click flags. Left = 1; Right = 2
extern volatile U32 MouseX; ///< Mouse X position relative to window
extern volatile U32 MouseY; ///< Mouse Y position relative to window
extern S32 MouseXDep;       ///< Mouse relative motion in the X axis
extern S32 MouseYDep;       ///< Mouse relative motion in the Y axis

/// Accumulated vertical mouse wheel delta (surface coords) since last consume.
extern volatile S32 MouseWheelYAccum;

extern S32 MouseSpriteGraphicNum;      ///< Id of graphic for mouse cursor
extern const void *MouseSpriteGraphic; ///< Memory of graphic for mouse cursor

/// True when the software cursor should be composited this frame (respects idle hide).
bool MouseSpriteShouldDraw(void);

/// True when mouse has moved since last call (edge-consuming).
bool MouseConsumeMotionFlag(void);

// --- Initialization ----------------------------------------------------------
void InitMouse();
void EndMouse();

// --- Interface ---------------------------------------------------------------
void ShowMouse(bool show);
/// Like ShowMouse(true), but the software sprite stays off until the next mouse motion (menus).
void ShowMouseAfterFirstMotion(void);
void SetMousePos(U32 x, U32 y);
void SetMouseBox(U32 x0, U32 y0, U32 x1, U32 y1); ///< Set mouse position limits
void ClearMouseBox();                             ///< Reset mouse position limits to screen size

void ManageMouse();
void HandleEventsMouse(const void *event);

/// Returns accumulated vertical wheel motion since last call and clears the accumulator.
S32 ConsumeMouseWheelY(void);

// =============================================================================
#ifdef __cplusplus
}
#endif
