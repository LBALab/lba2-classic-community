/* Host test for the console input/selection state machine. Drives the real
 * Console_UpdateKey path through the Console_Test* hooks (the same code the live
 * console runs after draining the SDL event queue), with no SDL or engine init.
 * Covers cursor editing, shift-selection, select-all, type-over-selection,
 * command history recall, tab completion, and two-stage Escape. */
#include "CONSOLE/CONSOLE.H"

#include <cstdio>
#include <cstring>

/* Link stubs: the console lib references these LIB386 symbols. */
extern "C" void AffStringToBuffer(U8 *dst, U32 pitch, S32 x, S32 y, const char *str, U8 ink) {
    (void)dst;
    (void)pitch;
    (void)x;
    (void)y;
    (void)str;
    (void)ink;
}
extern "C" void WindowToSurfaceCoords(S32 windowX, S32 windowY, S32 *surfaceX, S32 *surfaceY) {
    if (surfaceX)
        *surfaceX = windowX;
    if (surfaceY)
        *surfaceY = windowY;
}
void Console_RegisterAll(void) {}
int TryExecuteCheatByName(const char *name) {
    (void)name;
    return 0;
}

static void cmd_noop(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
}

static int g_failures = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__); \
            g_failures++;                                    \
        }                                                    \
    } while (0)

static void clear_input(void) {
    Console_TestCtrl('l'); /* Ctrl+L clears the input line */
}

int main(void) {
    int lo = 0, hi = 0;

    Console_Toggle(); /* open so the two-stage Escape test can close it */
    Console_RegisterCommand("cube", cmd_noop, "");
    Console_RegisterCommand("clear", cmd_noop, "");
    Console_RegisterCommand("close", cmd_noop, "");

    /* Typing and cursor motion. */
    clear_input();
    Console_TestType("hello");
    CHECK(strcmp(Console_TestInput(), "hello") == 0);
    CHECK(Console_TestCursor() == 5);
    Console_TestSpecial(CONSOLE_TKEY_BACKSPACE, 0);
    CHECK(strcmp(Console_TestInput(), "hell") == 0 && Console_TestCursor() == 4);
    Console_TestSpecial(CONSOLE_TKEY_HOME, 0);
    CHECK(Console_TestCursor() == 0);
    Console_TestType("X");
    CHECK(strcmp(Console_TestInput(), "Xhell") == 0 && Console_TestCursor() == 1);
    Console_TestSpecial(CONSOLE_TKEY_END, 0);
    CHECK(Console_TestCursor() == 5);
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 0);
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 0);
    CHECK(Console_TestCursor() == 3);
    Console_TestSpecial(CONSOLE_TKEY_DELETE, 0); /* "Xhell", cursor 3 -> delete [3]='l' */
    CHECK(strcmp(Console_TestInput(), "Xhel") == 0);

    /* Shift-selection then type-over. */
    clear_input();
    Console_TestType("abcdef");
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 1); /* Shift+Left x3 selects "def" */
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 1);
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 1);
    CHECK(Console_TestSelection(&lo, &hi) && lo == 3 && hi == 6);
    Console_TestType("Z"); /* typing replaces the selection */
    CHECK(strcmp(Console_TestInput(), "abcZ") == 0 && Console_TestCursor() == 4);
    CHECK(!Console_TestSelection(&lo, &hi));

    /* Plain Left collapses a selection to its near edge (no further move). */
    clear_input();
    Console_TestType("abcdef");
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 1);
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 1); /* select "ef", cursor at 4 */
    Console_TestSpecial(CONSOLE_TKEY_LEFT, 0); /* collapse to lo (4), not 3 */
    CHECK(!Console_TestSelection(&lo, &hi) && Console_TestCursor() == 4);

    /* Ctrl+A selects all; Backspace deletes the selection. */
    clear_input();
    Console_TestType("word");
    Console_TestCtrl('a');
    CHECK(Console_TestSelection(&lo, &hi) && lo == 0 && hi == 4);
    Console_TestSpecial(CONSOLE_TKEY_BACKSPACE, 0);
    CHECK(Console_TestInput()[0] == '\0');

    /* Command history recall. */
    clear_input();
    Console_TestType("alpha");
    Console_TestSpecial(CONSOLE_TKEY_ENTER, 0);
    Console_TestType("beta");
    Console_TestSpecial(CONSOLE_TKEY_ENTER, 0);
    Console_TestSpecial(CONSOLE_TKEY_UP, 0);
    CHECK(strcmp(Console_TestInput(), "beta") == 0);
    Console_TestSpecial(CONSOLE_TKEY_UP, 0);
    CHECK(strcmp(Console_TestInput(), "alpha") == 0);
    Console_TestSpecial(CONSOLE_TKEY_DOWN, 0);
    CHECK(strcmp(Console_TestInput(), "beta") == 0);
    Console_TestSpecial(CONSOLE_TKEY_DOWN, 0); /* past newest -> empty */
    CHECK(Console_TestInput()[0] == '\0');

    /* Tab completion: unique prefix completes and adds a space. */
    clear_input();
    Console_TestType("cu");
    Console_TestSpecial(CONSOLE_TKEY_TAB, 0);
    CHECK(strcmp(Console_TestInput(), "cube ") == 0);
    /* Ambiguous prefix extends to the longest common prefix only. */
    clear_input();
    Console_TestType("cl");
    Console_TestSpecial(CONSOLE_TKEY_TAB, 0);
    CHECK(strcmp(Console_TestInput(), "cl") == 0); /* clear / close -> "cl" */
    Console_TestType("o");
    Console_TestSpecial(CONSOLE_TKEY_TAB, 0);
    CHECK(strcmp(Console_TestInput(), "close ") == 0);

    /* Two-stage Escape: clear the line, then close the console. */
    clear_input();
    Console_TestType("x");
    Console_TestSpecial(CONSOLE_TKEY_ESC, 0);
    CHECK(Console_TestInput()[0] == '\0');
    CHECK(Console_IsOpen() == 1);
    Console_TestSpecial(CONSOLE_TKEY_ESC, 0);
    CHECK(Console_IsOpen() == 0);

    if (g_failures == 0)
        printf("test_console_input: OK\n");
    return g_failures != 0;
}
