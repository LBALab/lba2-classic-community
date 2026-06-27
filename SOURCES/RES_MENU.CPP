/*
 * Resolution catalog adapter for the Display options submenu. Turns the
 * shared RES_CATALOG menu list into labelled rows the menu can render. See
 * RES_MENU.H and docs/RUNTIME_RESOLUTION.md.
 *
 * The menu offers Res_BuildMenuList: the 640x480 classic anchor plus a ladder
 * of display-aspect-matched modes computed per monitor, so every entry fills
 * the player's panel. The longer curated/advanced list stays behind the console
 * `resolution` verb. Rows are labelled "WxH (aspect)" via Res_AspectLabel.
 */

#include "RES_MENU.H"

#include "RES_CATALOG.H"   /* Res_BuildMenuList, Res_AspectLabel */
#include <SVGA/SCREEN.H>   /* ModeDesiredX / ModeDesiredY */
#include <SYSTEM/WINDOW.H> /* Window_GetDisplayDimensions */

#include <stdio.h>

S32 Display_BuildEntries(DisplayEntry *out) {
    ResModeEntry cat[DISPLAY_MENU_MAX_ENTRIES];
    const int n = Res_BuildMenuList(cat, DISPLAY_MENU_MAX_ENTRIES);

    /* The ladder rungs are all matched to the panel aspect, so label them by the
       display, not by each rung's %8-snapped dimensions (which drift: 23:15 /
       139:90 / 208:135 for one 1.547:1 MacBook panel). The 640x480 classic
       anchor is genuinely 4:3, so it keeps its own label. */
    U32 dispW = 0, dispH = 0;
    const bool haveDisp = Window_GetDisplayDimensions(&dispW, &dispH);

    for (int i = 0; i < n; i++) {
        out[i].w = cat[i].w;
        out[i].h = cat[i].h;
        char aspect[16];
        const bool isAnchor = (cat[i].w == 640 && cat[i].h == 480);
        if (!isAnchor && haveDisp)
            Res_AspectLabel(dispW, dispH, aspect, sizeof aspect);
        else
            Res_AspectLabel(cat[i].w, cat[i].h, aspect, sizeof aspect);
        snprintf(out[i].label, sizeof out[i].label, "%ux%u (%s)",
                 (unsigned)cat[i].w, (unsigned)cat[i].h, aspect);
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
