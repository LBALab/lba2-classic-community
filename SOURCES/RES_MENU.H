/*
 * Resolution catalog adapter: helpers for the Display options submenu in
 * GAMEMENU.CPP (see docs/RUNTIME_RESOLUTION.md).
 *
 * A thin labelling layer over RES_CATALOG's menu list (Res_BuildMenuList): it
 * turns the 640x480 anchor plus the display-aspect-matched ladder into the
 * "WxH" rows the resolution sub-page renders (only the anchor is tagged
 * "(Classic)"). The curated and advanced lists stay behind the console
 * `resolution` verb.
 *
 * Rebuilt each time the sub-page opens, so a player who swaps monitors
 * mid-session gets a list matched to the new panel.
 */

#ifndef RES_MENU_H
#define RES_MENU_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Upper bound on rows the menu list can produce: the 640x480 anchor plus the
   aspect ladder (480/720/1080 rungs). 8 leaves generous headroom. Sizes the
   catalog scratch arrays and the runtime sub-page menu buffer. */
#define DISPLAY_MENU_MAX_ENTRIES 8

/* A single row in the resolution sub-page. label is the rendered text,
   e.g. "640x480 (Classic)" or "1280x720". */
typedef struct {
    U32 w;
    U32 h;
    char label[48];
} DisplayEntry;

/* Build the entry list from RES_CATALOG's per-monitor menu list (the 640x480
 * anchor plus the display-aspect ladder); the anchor is labelled "(Classic)",
 * the rest are bare "WxH". Returns count (<= cap). */
S32 Display_BuildEntries(DisplayEntry *out);

/* Index of the entry whose (w, h) matches ModeDesiredX/Y, or -1 if
 * the current resolution isn't in the list (e.g., player launched
 * with a custom --resolution / console `resolution` value). */
S32 Display_FindCurrentEntry(const DisplayEntry *entries, S32 count);

/* Convenience: write the current resolution's label (the value half,
 * "1280x720" or "640x480 (Classic)") into `out`. Used by the Display submenu
 * row to render "Resolution: %s". */
void Display_FormatCurrentLabel(char *out, S32 outSize);

#ifdef __cplusplus
}
#endif

#endif
