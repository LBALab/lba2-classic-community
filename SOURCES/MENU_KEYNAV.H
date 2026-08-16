#ifndef MENU_KEYNAV_H
#define MENU_KEYNAV_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Map a numeric-keypad scancode to the navigation key printed on it, and
 * return every other scancode untouched.
 *
 * With Num Lock off the keypad carries a second legend, and it is the one the
 * game's own default bindings assume: DefKeysDefault95 in INPUT.CPP pairs
 * every navigation binding with its keypad twin (I_UP is {K_GRAY_UP,
 * K_NUMPAD_8}, I_RETURN is {K_ENTER, K_NUMPAD_ENTER}). Anything reading the
 * Input flags therefore accepts the keypad for free.
 *
 * Screens that read raw scancodes instead do not, and each one has to restate
 * the mapping or silently lose the keypad. That is what this exists to stop:
 * call it once on MyKey and the rest of the screen can compare against the
 * navigation cluster alone.
 *
 * The mapping is the physical legend, nothing more: 7 Home, 8 Up, 9 PgUp,
 * 4 Left, 6 Right, 1 End, 2 Down, 3 PgDn, 0 Ins, . Del, Enter. Keypad 5 has no
 * navigation meaning and is left alone, as are + - * / which are punctuation
 * on both legends.
 *
 * It deliberately does not fold in a screen's own aliases (Tab for Right,
 * Space for Enter, Backspace for Delete). Those differ per screen and stay
 * where they are used. */
extern S32 MenuKey_NumpadToNav(S32 key);

#ifdef __cplusplus
}
#endif

#endif /* MENU_KEYNAV_H */
