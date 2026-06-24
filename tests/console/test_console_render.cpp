/* Host test for the console render path (docs/BOOT_LOG_PLAN.md boot-log shape):
 * per-line severity colour and UTF-8 -> CP437 transliteration.
 *
 * The real renderer (AffStringToBuffer) is replaced by a recording stub that
 * captures the (string, ink) passed for each drawn line. Console_PrePresent
 * runs Console_Draw against a small fake framebuffer + palette, so we assert:
 *   - the ink matches the line's CONSOLE_COL_* colour (severity as colour), and
 *   - the drawn bytes are CP437 (the "·" separator folded to 0xFA, "ç"/"ñ"
 *     to their CP437 bytes), never the raw UTF-8 lead/continuation bytes. */
#include "CONSOLE/CONSOLE.H"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct DrawCall {
    std::string str;
    U8 ink;
};
static std::vector<DrawCall> g_draws;

extern "C" void AffStringToBuffer(U8 *dst, U32 pitch, S32 x, S32 y, const char *str, U8 ink) {
    (void)dst;
    (void)pitch;
    (void)x;
    (void)y;
    DrawCall d;
    d.str = str ? str : "";
    d.ink = ink;
    g_draws.push_back(d);
}

/* Link stubs (the console lib references these; the render path doesn't use them). */
void Console_RegisterAll(void) {}
int TryExecuteCheatByName(const char *name) {
    (void)name;
    return 0;
}

/* Find the recorded draw for the line containing needle. */
static const DrawCall *draw_with(const char *needle) {
    for (size_t i = 0; i < g_draws.size(); i++)
        if (g_draws[i].str.find(needle) != std::string::npos)
            return &g_draws[i];
    return 0;
}

/* How many draws recorded the line containing needle (faux-bold = 2). */
static int count_draws(const char *needle) {
    int n = 0;
    for (size_t i = 0; i < g_draws.size(); i++)
        if (g_draws[i].str.find(needle) != std::string::npos)
            n++;
    return n;
}

static bool contains_byte(const std::string &s, unsigned char b) {
    return s.find((char)b) != std::string::npos;
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
    /* Open the console and push a representative mix of lines. */
    Console_Toggle();
    Console_Print("info-line");                           /* default colour */
    Console_PrintColored(CONSOLE_COL_DEBUG, "dbg-line");  /* grey */
    Console_PrintColored(CONSOLE_COL_WARN, "warn-line");  /* yellow */
    Console_PrintColored(CONSOLE_COL_ERROR, "err-line");  /* red */
    Console_PrintColored(CONSOLE_COL_BANNER, "bnr-line"); /* cyan bookend */
    Console_Print("LBA2 \xC2\xB7 x86_64");                /* UTF-8 middle dot */
    Console_Print("Fran\xC3\xA7"
                  "ais Espa\xC3\xB1ol"); /* ç (U+00E7), ñ (U+00F1) */

    /* Drive the render path against a small fake ARGB framebuffer + palette. */
    const U32 W = 320, H = 200;
    std::vector<U32> fb(W * H, 0);
    std::vector<U8> pal(256 * 3, 0);
    Console_PrePresent(fb.data(), W * sizeof(U32), W, H, pal.data());

    /* Severity/structure is rendered as the line's ink, not a text tag. */
    const DrawCall *info = draw_with("info-line");
    const DrawCall *dbg = draw_with("dbg-line");
    const DrawCall *warn = draw_with("warn-line");
    const DrawCall *err = draw_with("err-line");
    const DrawCall *bnr = draw_with("bnr-line");
    CHECK(info && info->ink == CONSOLE_COL_DEFAULT);
    CHECK(dbg && dbg->ink == CONSOLE_COL_DEBUG);
    CHECK(warn && warn->ink == CONSOLE_COL_WARN);
    CHECK(err && err->ink == CONSOLE_COL_ERROR);
    CHECK(bnr && bnr->ink == CONSOLE_COL_BANNER);

    /* Every scrollback line, banner included, is drawn exactly once: the banner
     * keeps its cyan ink but no longer gets a faux-bold second pass. */
    CHECK(count_draws("bnr-line") == 1);
    CHECK(count_draws("info-line") == 1);
    CHECK(count_draws("warn-line") == 1);

    /* The "·" reaches the font as CP437 0xFA, never the UTF-8 bytes 0xC2/0xB7. */
    const DrawCall *dot = draw_with("LBA2 ");
    CHECK(dot != 0);
    if (dot) {
        CHECK(contains_byte(dot->str, 0xFA));  /* CP437 middle dot */
        CHECK(!contains_byte(dot->str, 0xC2)); /* no UTF-8 lead byte */
        CHECK(!contains_byte(dot->str, 0xB7)); /* no UTF-8 continuation byte */
    }

    /* Accented language names fold to their CP437 bytes (ç=0x87, ñ=0xA4). */
    const DrawCall *lang = draw_with("Fran");
    CHECK(lang != 0);
    if (lang) {
        CHECK(contains_byte(lang->str, 0x87));  /* ç */
        CHECK(contains_byte(lang->str, 0xA4));  /* ñ */
        CHECK(!contains_byte(lang->str, 0xC3)); /* no UTF-8 lead byte */
    }

    if (g_failures == 0)
        printf("test_console_render: OK\n");
    return g_failures != 0;
}
