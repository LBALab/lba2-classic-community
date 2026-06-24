/* Host test for Console_CompletePrefix: tab-completion matching over the
 * registered command/cvar tables plus the built-ins (help/cmdlist/varlist).
 * Mirrors what the K_TAB handler runs in the live console. */
#include "CONSOLE/CONSOLE.H"

#include <cstdio>
#include <cstring>

/* Link stubs: the console lib references these, but completion doesn't use them. */
extern "C" void AffStringToBuffer(U8 *dst, U32 pitch, S32 x, S32 y, const char *str, U8 ink) {
    (void)dst;
    (void)pitch;
    (void)x;
    (void)y;
    (void)str;
    (void)ink;
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

int main(void) {
    int fps = 0;
    char out[256];
    const char *m[64];
    int n;

    Console_RegisterCommand("cube", cmd_noop, "");
    Console_RegisterCommand("load", cmd_noop, "");
    Console_RegisterCommand("loadbug", cmd_noop, "");
    Console_RegisterCommand("listsaves", cmd_noop, "");
    Console_RegisterCommand("listbugs", cmd_noop, "");
    Console_RegisterCvar("fps", &fps, CONSOLE_CVAR_INT, "");

    /* Unique prefix -> full name in out, a single match. */
    n = Console_CompletePrefix("cu", out, sizeof out, m, 64);
    CHECK(n == 1);
    CHECK(strcmp(out, "cube") == 0);

    /* Ambiguous prefix -> out is the longest common prefix of the matches. */
    n = Console_CompletePrefix("lo", out, sizeof out, m, 64);
    CHECK(n == 2); /* load, loadbug */
    CHECK(strcmp(out, "load") == 0);

    /* "l" matches load/loadbug/listsaves/listbugs -> common prefix is just "l". */
    n = Console_CompletePrefix("l", out, sizeof out, m, 64);
    CHECK(n == 4);
    CHECK(strcmp(out, "l") == 0);

    /* Cvars and built-ins are completion candidates too. */
    n = Console_CompletePrefix("fp", out, sizeof out, m, 64);
    CHECK(n == 1 && strcmp(out, "fps") == 0);
    n = Console_CompletePrefix("cmd", out, sizeof out, m, 64);
    CHECK(n == 1 && strcmp(out, "cmdlist") == 0); /* built-in */
    n = Console_CompletePrefix("var", out, sizeof out, m, 64);
    CHECK(n == 1 && strcmp(out, "varlist") == 0); /* built-in */

    /* No match leaves out empty and returns 0. */
    n = Console_CompletePrefix("zzz", out, sizeof out, m, 64);
    CHECK(n == 0);
    CHECK(out[0] == '\0');

    /* Empty prefix matches every candidate: 5 cmds + 1 cvar + 3 built-ins. */
    n = Console_CompletePrefix("", out, sizeof out, m, 64);
    CHECK(n == 5 + 1 + 3);

    if (g_failures == 0)
        printf("test_console_complete: OK\n");
    return g_failures != 0;
}
