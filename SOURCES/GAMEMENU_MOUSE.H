#ifndef GAMEMENU_MOUSE_H
#define GAMEMENU_MOUSE_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

void MenuMouseAcquire(void);
void MenuMouseRelease(void);
void MenuMouseApplyCursorSprite(void);
void GameMenuMouseResetPress(void);
void GameMenuMousePollFocus(void);

/// Non-zero when menu mouse cursor should be active (config + refcount).
S32 MenuMouse_GetRefCount(void);

/// Hit-test which menu row (0..nb-1) contains (mx,my), or -1. Mirrors DrawGameMenu geometry.
S32 GameMenuHitTestRow(const U16 *ptrmenu, U32 mx, U32 my, const U16 *gameMainMenuPtr);

/**
 * After MyGetInput in DoGameMenu: hover, press/release, optional return.
 * @return -1 = no action; >= 0 = return this menu mess num (same as Enter on current row).
 */
S32 GameMenuMouseProcessDoGameMenu(U16 *ptrmenu, S16 *selected, S32 *flag);

#ifdef __cplusplus
}
#endif

#endif
