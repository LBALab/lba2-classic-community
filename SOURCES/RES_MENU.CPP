/*
 * Resolution catalog adapter for the Display options submenu. Turns the
 * shared RES_CATALOG menu list into labelled rows the menu can render. See
 * RES_MENU.H and docs/RUNTIME_RESOLUTION.md.
 *
 * The menu offers Res_BuildMenuList: the 640x480 classic anchor plus a ladder
 * of display-aspect-matched modes computed per monitor, so every entry fills
 * the player's panel. The longer curated/advanced list stays behind the console
 * `resolution` verb. Rows are bare "WxH"; only the anchor is tagged "(Classic)"
 * to flag the original 4:3 mode that pillarboxes on a widescreen panel.
 */

#include "RES_MENU.H"

#include "RES_CATALOG.H" /* Res_BuildMenuList */
#include <SVGA/SCREEN.H> /* ModeDesiredX / ModeDesiredY */

#include <stdio.h>
#include <string.h>

S32 Display_BuildEntries(DisplayEntry *out) {
    ResModeEntry cat[DISPLAY_MENU_MAX_ENTRIES];
    const int n = Res_BuildMenuList(cat, DISPLAY_MENU_MAX_ENTRIES);

    for (int i = 0; i < n; i++) {
        out[i].w = cat[i].w;
        out[i].h = cat[i].h;
        /* Only the classic anchor is tagged: it is the original 4:3 mode and
           pillarboxes on a widescreen panel, so the "(Classic)" hint flags that
           it behaves differently. The display-aspect ladder rows are bare WxH;
           they all fill the panel, and the current one is already green-marked.
           Keyed on the catalog tag, so the anchor identity lives in RES_CATALOG
           rather than a 640x480 literal duplicated here. */
        if (strcmp(cat[i].tag, "classic") == 0)
            snprintf(out[i].label, sizeof out[i].label, "%ux%u (Classic)",
                     (unsigned)cat[i].w, (unsigned)cat[i].h);
        else
            snprintf(out[i].label, sizeof out[i].label, "%ux%u",
                     (unsigned)cat[i].w, (unsigned)cat[i].h);
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
