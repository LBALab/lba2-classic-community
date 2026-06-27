/*
 * Resolution catalog adapter for the Display options submenu. Turns the
 * shared RES_CATALOG mode list into labelled rows the menu can render. See
 * RES_MENU.H and docs/RUNTIME_RESOLUTION.md.
 *
 * The menu offers the recommended tier (the modes that fit the current
 * display, plus a synthesised native entry), HD included. The longer
 * "advanced" list stays behind the console `resolution --all` verb. Rows are
 * labelled "WxH (a:b)"; the catalog's provenance tags are console-only.
 */

#include "RES_MENU.H"

#include "RES_CATALOG.H" /* ResModeEntry, Res_BuildCatalog, Res_Aspect */
#include <SVGA/SCREEN.H> /* ModeDesiredX / ModeDesiredY */

#include <stdio.h>

S32 Display_BuildEntries(DisplayEntry *out) {
    ResModeEntry cat[DISPLAY_MENU_MAX_ENTRIES];
    const int n = Res_BuildCatalog(cat, DISPLAY_MENU_MAX_ENTRIES, false /* recommended tier */);

    for (int i = 0; i < n; i++) {
        out[i].w = cat[i].w;
        out[i].h = cat[i].h;
        U32 a = 0, b = 0;
        Res_Aspect(cat[i].w, cat[i].h, &a, &b);
        snprintf(out[i].label, sizeof out[i].label, "%ux%u (%u:%u)",
                 (unsigned)cat[i].w, (unsigned)cat[i].h, (unsigned)a, (unsigned)b);
    }
    return (S32)n;
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
        /* Not in the curated list: show the bare dimensions so the value
           isn't blank. Reachable if the player launched at a console-verb
           resolution that the recommended tier doesn't enumerate. */
        snprintf(out, (size_t)outSize, "%ux%u",
                 (unsigned)ModeDesiredX, (unsigned)ModeDesiredY);
    }
}
