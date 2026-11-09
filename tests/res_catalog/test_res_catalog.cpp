/* Host contract test for the resolution lists: every mode the console verb or
 * the Display menu offers must be one the engine can switch to. The console
 * once listed 1366x768, a width Res_ValidateDimensions refuses, and choosing
 * it started the game at 640x480.
 *
 * The display size is faked. With no display, Res_BuildCatalog skips its
 * fit-to-display filter and returns every table entry; the panel sizes below
 * exercise the widths both builders compute from the display. */

#include "RES_CATALOG.H"
#include <SYSTEM/WINDOW.H>

#include <cstdio>

static bool haveDisplay = false;
static U32 displayW = 0;
static U32 displayH = 0;

bool Window_GetDisplayDimensions(U32 *outW, U32 *outH) {
    if (!haveDisplay)
        return false;
    *outW = displayW;
    *outH = displayH;
    return true;
}

static int failures = 0;

/* The rule Res_ValidateDimensions (RES_SWITCH.CPP) applies. That file needs
   the whole engine to link, so the rule is restated from the same bounds. */
static bool switchAccepts(U32 w, U32 h) {
    return w >= RES_MIN_W && w <= RES_MAX_W && h >= RES_MIN_H && h <= RES_MAX_H &&
           (w & 7) == 0;
}

static void checkList(const char *what, const ResModeEntry *list, int count) {
    if (count < 1 || list[0].w != 640 || list[0].h != 480) {
        std::printf("FAIL %s: 640x480 is not the first entry\n", what);
        failures++;
    }
    for (int i = 0; i < count; i++) {
        if (!switchAccepts(list[i].w, list[i].h)) {
            std::printf("FAIL %s: %ux%u (%s) is not a size the engine runs at\n", what,
                        list[i].w, list[i].h, list[i].tag);
            failures++;
        }
        for (int j = 0; j < i; j++) {
            if (list[j].w == list[i].w && list[j].h == list[i].h) {
                std::printf("FAIL %s: %ux%u listed twice\n", what, list[i].w, list[i].h);
                failures++;
            }
        }
    }
}

static void checkDisplay(bool present, U32 w, U32 h) {
    haveDisplay = present;
    displayW = w;
    displayH = h;

    char what[64];
    ResModeEntry list[64];
    int count;

    std::snprintf(what, sizeof what, "catalog --all, display %ux%u", w, h);
    if (!present)
        std::snprintf(what, sizeof what, "catalog --all, no display");
    count = Res_BuildCatalog(list, 64, true);
    checkList(what, list, count);

    if (!present)
        return; /* the menu list is just the anchor without a display */
    std::snprintf(what, sizeof what, "menu list, display %ux%u", w, h);
    count = Res_BuildMenuList(list, 64);
    checkList(what, list, count);
}

int main() {
    checkDisplay(false, 0, 0);

    const U32 panels[][2] = {
        {1366, 768}, /* the panel that 1366x768 was meant for */
        {1920, 1080},
        {2560, 1440},
        {2560, 1600},
        {3440, 1440},
        {3840, 2160},
        {5120, 1440},
        {1280, 800},
        {800, 600},
    };
    for (unsigned i = 0; i < sizeof panels / sizeof panels[0]; i++)
        checkDisplay(true, panels[i][0], panels[i][1]);

    if (failures != 0) {
        std::printf("test_res_catalog: %d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("test_res_catalog: ok\n");
    return 0;
}
