/* Host contract test for the Display menu's resolution label and current-row
 * marker. An interior may render below the player's selected resolution, but
 * the menu must continue to describe and mark the selected base resolution. */

#include "RES_CATALOG.H"
#include "RES_MENU.H"
#include "RES_SWITCH.H"
#include <SVGA/SCREEN.H>

#include <cstdio>
#include <cstring>

static U32 baseW = 1280;
static U32 baseH = 720;

U32 ModeDesiredX = 640;
U32 ModeDesiredY = 360;

void Res_BaseDimensions(U32 *outW, U32 *outH) {
    *outW = baseW;
    *outH = baseH;
}

int Res_BuildMenuList(ResModeEntry *out, int cap) {
    if (cap < 2)
        return 0;

    out[0].w = 640;
    out[0].h = 480;
    out[0].tag = "classic";
    out[1].w = 1280;
    out[1].h = 720;
    out[1].tag = "recommended";
    return 2;
}

static int failures = 0;

static void expectEqual(const char *label, const char *got, const char *want) {
    if (std::strcmp(got, want) != 0) {
        std::printf("FAIL %s: got \"%s\", want \"%s\"\n", label, got, want);
        failures++;
    }
}

int main() {
    DisplayEntry entries[DISPLAY_MENU_MAX_ENTRIES];
    const S32 count = Display_BuildEntries(entries);
    char label[48];

    /* Simulates a 1280x720 choice rendering an interior at 640x360. */
    baseW = 1280;
    baseH = 720;
    if (Display_FindCurrentEntry(entries, count) != 1) {
        std::printf("FAIL selected row: 1280x720 was not marked current\n");
        failures++;
    }
    Display_FormatCurrentLabel(label, sizeof label);
    expectEqual("selected label", label, "1280x720");

    baseW = 640;
    baseH = 480;
    Display_FormatCurrentLabel(label, sizeof label);
    expectEqual("classic label", label, "640x480 (Classic)");

    baseW = 1024;
    baseH = 576;
    if (Display_FindCurrentEntry(entries, count) != -1) {
        std::printf("FAIL custom row: unexpected catalog match\n");
        failures++;
    }
    Display_FormatCurrentLabel(label, sizeof label);
    expectEqual("custom fallback", label, "1024x576");

    if (failures != 0) {
        std::printf("test_res_menu: %d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("test_res_menu: ok\n");
    return 0;
}
