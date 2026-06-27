/*
 * Resolution catalog adapter: helpers for the Display options submenu in
 * GAMEMENU.CPP (see docs/RUNTIME_RESOLUTION.md).
 *
 * A thin labelling layer over the shared RES_CATALOG mode list: it turns the
 * recommended tier (display-fitting modes + a synthesised native entry, HD
 * included) into "WxH (a:b)" rows the resolution sub-page can render, so the
 * menu code doesn't have to query the SDL display or reduce aspect ratios.
 * The longer advanced list stays behind the console `resolution --all` verb.
 *
 * Rebuilt each time the sub-page opens, so a player who swaps monitors
 * mid-session gets a refreshed list.
 */

#ifndef RES_MENU_H
#define RES_MENU_H

#include <SYSTEM/ADELINE_TYPES.H>

#ifdef __cplusplus
extern "C" {
#endif

/* Upper bound on rows the recommended tier can produce: the curated
   recommended modes (currently 5) plus one synthesised native entry. Sizes
   the catalog scratch arrays and the runtime sub-page menu buffer. */
#define DISPLAY_MENU_MAX_ENTRIES 8

/* A single row in the resolution sub-page. label is the rendered text,
   e.g. "640x480 (4:3)" or "1280x720 (16:9)". */
typedef struct {
    U32 w;
    U32 h;
    char label[48];
} DisplayEntry;

/* Build the entry list from the shared catalog's recommended tier. Always
 * emits 640x480 at index 0; appends the display-fitting modes and a native
 * entry when the display query succeeds. Returns count (<= cap). */
S32 Display_BuildEntries(DisplayEntry *out);

/* Index of the entry whose (w, h) matches ModeDesiredX/Y, or -1 if
 * the current resolution isn't in the list (e.g., player launched
 * with a custom --resolution / console `resolution` value). */
S32 Display_FindCurrentEntry(const DisplayEntry *entries, S32 count);

/* Convenience: write the current resolution's label (the value half,
 * "1280x720 (16:9)" etc.) into `out`. Used by the Display submenu row to
 * render "Resolution: %s". */
void Display_FormatCurrentLabel(char *out, S32 outSize);

#ifdef __cplusplus
}
#endif

#endif
