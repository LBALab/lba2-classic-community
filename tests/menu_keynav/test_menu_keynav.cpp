/* Host test: the numeric keypad reaches menus that read raw scancodes.
 *
 * The game's default bindings already pair every navigation key with its
 * keypad twin (DefKeysDefault95 in SOURCES/INPUT.CPP: I_UP is {K_GRAY_UP,
 * K_NUMPAD_8}, I_RETURN is {K_ENTER, K_NUMPAD_ENTER}), so any screen driven by
 * the Input flags accepts the keypad for free. Screens that compare raw MyKey
 * instead have to restate that mapping, and one that restates it partially
 * loses the keys it forgot -- which is how ConfirmMenu came to accept the
 * keypad for moving between OK and Cancel while refusing keypad Enter to press
 * either of them (#497 was the same mistake one layer up, in keycode space).
 *
 * MenuKey_NumpadToNav is the single statement of that mapping. This pins it:
 * the whole Num-Lock-off legend maps, everything else is returned untouched,
 * and the function is idempotent so a screen calling it cannot corrupt a key
 * that was already in the navigation cluster.
 *
 * Lean link: only SOURCES/MENU_KEYNAV.CPP. No SDL calls, no game stack.
 */
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include "MENU_KEYNAV.H"

#include <cstdio>

static int g_failures = 0;

static void check(const char *what, S32 got, S32 want) {
    if (got != want) {
        printf("FAIL: %s -> %d, expected %d\n", what, (int)got, (int)want);
        g_failures++;
    }
}

int main(void) {
    /* The Num-Lock-off legend, key by key. */
    check("keypad 7", MenuKey_NumpadToNav(K_NUMPAD_7), K_HOME);
    check("keypad 8", MenuKey_NumpadToNav(K_NUMPAD_8), K_UP);
    check("keypad 9", MenuKey_NumpadToNav(K_NUMPAD_9), K_PAGE_UP);
    check("keypad 4", MenuKey_NumpadToNav(K_NUMPAD_4), K_LEFT);
    check("keypad 6", MenuKey_NumpadToNav(K_NUMPAD_6), K_RIGHT);
    check("keypad 1", MenuKey_NumpadToNav(K_NUMPAD_1), K_END);
    check("keypad 2", MenuKey_NumpadToNav(K_NUMPAD_2), K_DOWN);
    check("keypad 3", MenuKey_NumpadToNav(K_NUMPAD_3), K_PAGE_DOWN);
    check("keypad 0", MenuKey_NumpadToNav(K_NUMPAD_0), K_INSER);
    check("keypad .", MenuKey_NumpadToNav(K_NUMPAD_POINT), K_SUPPR);

    /* The regression this exists for: keypad Enter must become Enter, so a
       screen testing K_ENTER alone still confirms on it. */
    check("keypad Enter", MenuKey_NumpadToNav(K_NUMPAD_ENTER), K_ENTER);

    /* Keys with no navigation legend are left alone. Keypad 5 is the dead
       centre of the cluster, and the four operators are punctuation on both
       legends. */
    check("keypad 5", MenuKey_NumpadToNav(K_NUMPAD_5), K_NUMPAD_5);
    check("keypad +", MenuKey_NumpadToNav(K_NUMPAD_PLUS), K_NUMPAD_PLUS);
    check("keypad -", MenuKey_NumpadToNav(K_NUMPAD_MOINS), K_NUMPAD_MOINS);
    check("keypad *", MenuKey_NumpadToNav(K_NUMPAD_MUL), K_NUMPAD_MUL);
    check("keypad /", MenuKey_NumpadToNav(K_NUMPAD_DIV), K_NUMPAD_DIV);

    /* Everything off the keypad passes through unchanged, so a screen can call
       this on every key it reads without auditing the rest of its switch. */
    check("Esc", MenuKey_NumpadToNav(K_ESC), K_ESC);
    check("Enter", MenuKey_NumpadToNav(K_ENTER), K_ENTER);
    check("Space", MenuKey_NumpadToNav(K_SPACE), K_SPACE);
    check("Tab", MenuKey_NumpadToNav(K_TAB), K_TAB);
    check("Backspace", MenuKey_NumpadToNav(K_BACKSPACE), K_BACKSPACE);
    check("A", MenuKey_NumpadToNav(K_A), K_A);
    check("F10", MenuKey_NumpadToNav(K_F10), K_F10);
    check("nothing held", MenuKey_NumpadToNav(0), 0);

    /* The arrow cluster is already the target of the mapping, so mapping it
       again must be a no-op. Idempotence is what lets a caller apply this
       unconditionally, including on a key another layer already translated
       (the save menu synthesises K_GRAY_UP/DOWN from the gamepad). */
    check("Up", MenuKey_NumpadToNav(K_UP), K_UP);
    check("Down", MenuKey_NumpadToNav(K_DOWN), K_DOWN);
    check("Left", MenuKey_NumpadToNav(K_LEFT), K_LEFT);
    check("Right", MenuKey_NumpadToNav(K_RIGHT), K_RIGHT);
    check("Home", MenuKey_NumpadToNav(K_HOME), K_HOME);
    check("End", MenuKey_NumpadToNav(K_END), K_END);
    check("PageUp", MenuKey_NumpadToNav(K_PAGE_UP), K_PAGE_UP);
    check("PageDown", MenuKey_NumpadToNav(K_PAGE_DOWN), K_PAGE_DOWN);
    check("Insert", MenuKey_NumpadToNav(K_INSER), K_INSER);
    check("Delete", MenuKey_NumpadToNav(K_SUPPR), K_SUPPR);
    check("twice", MenuKey_NumpadToNav(MenuKey_NumpadToNav(K_NUMPAD_ENTER)), K_ENTER);

    if (g_failures) {
        printf("test_menu_keynav: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("test_menu_keynav: OK\n");
    return 0;
}
