#ifndef MENU_KEYNAV_H
#define MENU_KEYNAV_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Map a numeric-keypad scancode to the navigation key printed on it, and
 * return every other scancode untouched.
 *
 * With Num Lock off the keypad carries a second legend. The Input layer covers
 * part of it: DefKeysDefault95 in INPUT.CPP gives a keypad twin to the four
 * directions, I_ACTION_M and I_RETURN, so a screen reading the Input flags
 * accepts the keypad for those. It stops there. Home, End, Page Up, Page Down,
 * Insert and Delete have no Input action at all, and two entries run the other
 * way (I_ACTION_ALWAYS is {K_W, K_GRAY_END}), so no amount of reading Input
 * gets a screen the rest of the legend.
 *
 * Screens comparing raw scancodes have to state the mapping themselves, and one
 * that states it partially silently loses the keys it left out. That is what
 * this exists to stop: call it once on MyKey and the rest of the screen can
 * compare against the navigation cluster alone.
 *
 * The mapping is the physical legend, nothing more: 7 Home, 8 Up, 9 PgUp,
 * 4 Left, 6 Right, 1 End, 2 Down, 3 PgDn, 0 Ins, . Del, Enter. Keypad 5 has no
 * navigation meaning and is left alone, as are + - * / which are punctuation
 * on both legends.
 *
 * It deliberately does not fold in a screen's own aliases (Tab for Right,
 * Space for Enter, Backspace for Delete). Those differ per screen and stay
 * where they are used.
 *
 * FOR MENU NAVIGATION ONLY. The legend is applied unconditionally, without
 * consulting Num Lock, because SDL reports the same scancode either way and a
 * menu wants the navigation meaning in both states. A screen that accepts typed
 * text or digits must not call this: it would turn the player's keypad digits
 * into cursor movement. Such a screen reads characters through GetAscii, where
 * the keypad question is a different one (see A_IS_CHARACTER). */
extern S32 MenuKey_NumpadToNav(S32 key);

#ifdef __cplusplus
}
#endif

#endif /* MENU_KEYNAV_H */
