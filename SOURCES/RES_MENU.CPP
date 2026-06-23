/*
 * Resolution catalog helpers for the Display options submenu. See
 * RES_MENU.H for the API summary and docs/RUNTIME_RESOLUTION.md for
 * the Phase 2 context.
 *
 * Two-entry shape by design — Classic + a display-aspect-derived
 * Widescreen at 480 height. HD modes deferred until A4/A5 (terrain
 * horizon + holomap Z-buffer Y clamps) land. Arbitrary widths remain
 * available via the console `resolution` verb.
 */

#include "RES_MENU.H"

#include "RES_SWITCH.H"    /* Res_SnapWidescreenWidth (shared with the boot path) */
#include <SVGA/SCREEN.H>   /* ModeDesiredX / ModeDesiredY */
#include <SYSTEM/WINDOW.H> /* Window_GetDisplayDimensions */

#include <stdio.h>
#include <string.h>

/* GCD-reduce w,h to its (a:b) form. Same as res_aspect in CONSOLE_CMD.CPP. */
static void aspect_ratio(U32 w, U32 h, U32 *outA, U32 *outB) {
    U32 a = w, b = h;
    while (b) {
        U32 t = b;
        b = a % b;
        a = t;
    }
    if (a == 0)
        a = 1;
    *outA = w / a;
    *outB = h / a;
}

S32 Display_BuildEntries(DisplayEntry *out) {
    S32 n = 0;

    /* Classic anchor — always emitted. */
    out[n].w = 640;
    out[n].h = 480;
    snprintf(out[n].label, sizeof out[n].label, "Classic 4:3 (640x480)");
    n++;

    U32 dispW = 0, dispH = 0;
    if (!Window_GetDisplayDimensions(&dispW, &dispH))
        return n;

    U32 wsW = Res_SnapWidescreenWidth(dispW, dispH);
    if (wsW == 0 || wsW <= 640)
        return n;

    U32 ar_a, ar_b;
    aspect_ratio(dispW, dispH, &ar_a, &ar_b);

    snprintf(out[n].label, sizeof out[n].label,
             "Widescreen %u:%u (%ux480)", ar_a, ar_b, wsW);
    out[n].w = wsW;
    out[n].h = 480;
    n++;

    return n;
}

S32 Display_FindCurrentEntry(const DisplayEntry *entries, S32 count) {
    for (S32 i = 0; i < count; i++) {
        if (entries[i].w == ModeDesiredX && entries[i].h == ModeDesiredY)
            return i;
    }
    return -1;
}

void Display_FormatCurrentLabel(char *out, S32 outSize) {
    DisplayEntry entries[DISPLAY_MENU_MAX_ENTRIES];
    S32 count = Display_BuildEntries(entries);
    S32 idx = Display_FindCurrentEntry(entries, count);
    if (idx >= 0) {
        snprintf(out, (size_t)outSize, "%s", entries[idx].label);
    } else {
        /* Not in the curated list — show the bare dimensions so the
           player isn't confused by an empty value. Reachable if the
           player launched at a console-verb resolution like 1024x480. */
        snprintf(out, (size_t)outSize, "%ux%u",
                 (unsigned)ModeDesiredX, (unsigned)ModeDesiredY);
    }
}
