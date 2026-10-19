#ifndef MENU_INPUT_H
#define MENU_INPUT_H

#include <SYSTEM/ADELINE_TYPES.H>

#define MENU_INPUT_COUNT 4
#define MENU_INPUT_UP (1u << 0)
#define MENU_INPUT_DOWN (1u << 1)
#define MENU_INPUT_LEFT (1u << 2)
#define MENU_INPUT_RIGHT (1u << 3)
#define MENU_INPUT_ALL (MENU_INPUT_UP | MENU_INPUT_DOWN | MENU_INPUT_LEFT | MENU_INPUT_RIGHT)
#define MENU_INPUT_ISOLATED (1u << MENU_INPUT_COUNT)

/* Fixed arrows/keypad and left stick/D-pad, sampled without another keyboard/replay poll. */
extern void MenuInput_Poll(S32 isolated);
extern U32 MenuInput_Get(void);
extern U32 MenuInput_Held(void);
extern void MenuInput_Suppress(U32 directions);
extern U32 MenuInput_Suppressed(void);
extern void MenuInput_Reset(U32 suppressed);
extern S32 MenuInput_NavigationKey(void);
/* Poll-stream sample: physical directions plus console isolation. -1 ends replay injection. */
extern U32 MenuInput_Capture(S32 isolated, U32 reservedKey);
extern void MenuInput_SetReplaySample(S32 sample);

#endif
