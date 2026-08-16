/* Host test: MenuKey_NumpadToNav is the single statement of the Num-Lock-off
 * keypad legend, for the menus that compare raw scancodes.
 *
 * The Input layer carries a keypad twin for the four directions, I_ACTION_M and
 * I_RETURN, and nothing else; Home, End, Page Up, Page Down, Insert and Delete
 * have no Input action at all. A screen reading raw MyKey therefore depends
 * entirely on this mapping for most of the legend, and a screen that states the
 * mapping itself will state it partially.
 *
 * Pinned here: every key of the legend maps, including both scancodes the
 * keypad period arrives as; everything else is returned untouched, so a caller
 * may apply it to every key it reads; and it is idempotent, so a key another
 * layer already translated survives it.
 *
 * Lean link: only SOURCES/MENU_KEYNAV.CPP. No SDL calls, no game stack.
 */
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include <SDL3/SDL_scancode.h>

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

    /* The keypad period reaches the engine as KP_PERIOD from a standard PC
       keypad and as KP_DECIMAL from the extended block. Asserting the raw SDL
       constants rather than the K_ aliases, because an alias pointing at the
       scancode the hardware does not send is exactly the failure this pair
       guards, and testing through the alias would agree with itself. */
    check("keypad . (KP_PERIOD)", MenuKey_NumpadToNav(SDL_SCANCODE_KP_PERIOD), K_SUPPR);
    check("keypad . (KP_DECIMAL)", MenuKey_NumpadToNav(SDL_SCANCODE_KP_DECIMAL), K_SUPPR);

    /* A screen testing K_ENTER alone must still confirm on the keypad's own
       Enter. */
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

    /* MyKey carries a gamepad virtual scancode whenever no keyboard key is
       down, and every caller passes that same MyKey through here, so the
       extended region above K_GAMEPAD_BASE has to survive untouched. */
    check("pad A", MenuKey_NumpadToNav(K_GAMEPAD_A), K_GAMEPAD_A);
    check("pad B", MenuKey_NumpadToNav(K_GAMEPAD_B), K_GAMEPAD_B);
    check("pad base", MenuKey_NumpadToNav(K_GAMEPAD_BASE), K_GAMEPAD_BASE);

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
