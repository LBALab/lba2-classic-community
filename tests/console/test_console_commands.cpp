#include "CONSOLE/CONSOLE.H"
#include "CONSOLE/CONSOLE_STATE.H"

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> g_lines;

extern "C" void AffStringToBuffer(U8 *dst, U32 pitch, S32 x, S32 y, const char *str, U8 ink) {
    (void)dst;
    (void)pitch;
    (void)x;
    (void)y;
    (void)str;
    (void)ink;
}

void Console_RegisterAll(void) {
}

int TryExecuteCheatByName(const char *name) {
    (void)name;
    return 0;
}

static void test_line_sink(const char *line) {
    g_lines.push_back(line ? line : "");
}

static void clear_lines() {
    g_lines.clear();
}

static bool has_line_containing(const char *needle) {
    for (size_t i = 0; i < g_lines.size(); i++) {
        if (g_lines[i].find(needle) != std::string::npos)
            return true;
    }
    return false;
}

static void cmd_status_for_test(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    S16 vars[256];
    memset(vars, 0, sizeof(vars));
    vars[CONSOLE_LISTVAR_FLAG_CHAPTER] = 7;
    char line[128];
    Console_FormatStatusIslandLine_FromState(line, sizeof(line), 10, 3, vars, CONSOLE_LISTVAR_FLAG_CHAPTER);
    Console_Print("%s", line);
}

int main(void) {
    int fps = 0;

    Console_SetLineSinkForTests(test_line_sink);
    Console_RegisterCommand("status", cmd_status_for_test, "Print test status line");
    Console_RegisterCvar("fps", &fps, CONSOLE_CVAR_INT, "Frame rate display");

    clear_lines();
    Console_Execute("help status");
    assert(has_line_containing("status - Print test status line"));

    clear_lines();
    Console_Execute("cmdlist");
    assert(has_line_containing("status"));

    clear_lines();
    Console_Execute("status");
    assert(has_line_containing("Island: 10  Cube: 3  Chapter: 7"));

    clear_lines();
    Console_Execute("fps 1");
    assert(fps == 1);
    assert(has_line_containing("fps = 1"));

    clear_lines();
    Console_Execute("unknown_thing");
    assert(has_line_containing("Unknown command: unknown_thing"));

    /* Console_Print splits on '\n' so callers can pass multi-line strings
     * naturally (matches printf intuition). Empty segments — from leading,
     * trailing, or repeated '\n' — collapse so the user doesn't see
     * spurious blank lines. */
    clear_lines();
    Console_Print("first\nsecond\nthird");
    assert(g_lines.size() == 3);
    assert(g_lines[0] == "first");
    assert(g_lines[1] == "second");
    assert(g_lines[2] == "third");

    clear_lines();
    Console_Print("\nfoo\n\nbar\n");
    assert(g_lines.size() == 2);
    assert(g_lines[0] == "foo");
    assert(g_lines[1] == "bar");

    clear_lines();
    Console_Print("single line, no newline");
    assert(g_lines.size() == 1);
    assert(g_lines[0] == "single line, no newline");

    Console_SetLineSinkForTests(NULL);
    return 0;
}
