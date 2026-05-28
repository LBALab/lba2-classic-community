/*
 * Resolution catalog — helpers for the Display options submenu in
 * GAMEMENU.CPP. Phase 2 of the runtime-resolution work (see
 * docs/RUNTIME_RESOLUTION.md).
 *
 * Sibling to the console `resolution` verb (CONSOLE_CMD.CPP, which
 * builds a longer recommended-plus-advanced list). This module is
 * deliberately narrower: just the two-entry shape — Classic + a
 * display-aspect-derived Widescreen — that the in-game menu offers,
 * factored out so the menu code in GAMEMENU.CPP doesn't have to know
 * how to query the SDL display or compute aspect ratios.
 *
 * The catalog is rebuilt on every CycleResolution call so a player
 * who swaps monitors mid-session gets a refreshed Widescreen entry.
 */

#ifndef RES_MENU_H
#define RES_MENU_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_MENU_MAX_ENTRIES 4

/* A single row in the Display submenu. label is the full rendered
   text (e.g. "Classic 4:3 (640x480)" or "Widescreen 16:9 (848x480)"). */
typedef struct {
    U32 w;
    U32 h;
    char label[48];
} DisplayEntry;

/* Build the entry list. Always emits Classic at index 0; appends the
 * display-derived Widescreen entry when Window_GetDisplayDimensions
 * succeeds and yields a wider-than-640 snapped width. Returns count. */
S32 Display_BuildEntries(DisplayEntry *out);

/* Index of the entry whose (w, h) matches ModeDesiredX/Y, or -1 if
 * the current resolution isn't in the list (e.g., player launched
 * with a custom --resolution / console `resolution` value). */
S32 Display_FindCurrentEntry(const DisplayEntry *entries, S32 count);

/* Convenience: write the current resolution's label (just the value
 * half — "Classic 4:3 (640x480)" etc.) into `out`. Used by the menu
 * cycle row to render "Resolution: %s". */
void Display_FormatCurrentLabel(char *out, S32 outSize);

#ifdef __cplusplus
}
#endif

#endif
