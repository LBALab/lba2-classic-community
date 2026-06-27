/*
 * Resolution catalog: the shared list of selectable render resolutions and
 * the helpers that compose it. One home for the data and rules that used to
 * live duplicated in the console `resolution` verb (CONSOLE_CMD.CPP) and the
 * Display options submenu (RES_MENU.CPP):
 *
 *   - the canonical (W, H) bounds and the multiple-of-8 width rule,
 *   - the curated recommended / advanced mode tables,
 *   - aspect-ratio reduction and display-native snapping,
 *   - the filter-to-display, dedupe, append-native list builder.
 *
 * Consumers layer their own presentation on top: the console prints the list
 * with tags; the menu formats "WxH (a:b)" rows; RES_SWITCH validates against
 * the same bounds. See docs/RUNTIME_RESOLUTION.md.
 */

#pragma once

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/LIMITS.H> /* ADELINE_MAX_Y_RES, the framebuffer Y ceiling */

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical render-resolution bounds. Width must additionally be a multiple
   of 8. Single source of truth: RES_SWITCH (Res_ValidateDimensions), the
   --resolution CLI parser, the console verb, and the Display submenu all
   derive their limits from these. The Y ceiling is ADELINE_MAX_Y_RES so the
   bound and the static TabOffLine[]/TabOffPhysLine[] sizing can't drift. */
#define RES_MIN_W 320
#define RES_MAX_W 1920
#define RES_MIN_H 200
#define RES_MAX_H ADELINE_MAX_Y_RES

/* One selectable mode. `tag` is a short provenance word ("classic",
   "recommended", "experimental", "native", "advanced") used by the console
   listing; the menu ignores it and formats its own labels. Points at a
   static literal; never freed. */
typedef struct {
    U32 w, h;
    const char *tag;
} ResModeEntry;

/* GCD-reduce (w, h) to its aspect, e.g. 1280x720 -> 16:9. Writes 1:1-style
   fallback when h is zero. */
void Res_Aspect(U32 w, U32 h, U32 *outA, U32 *outB);

/* Snap a display dimension into the supported range. Width additionally
   rounds down to the nearest multiple of 8 (e.g. 1366 -> 1360). Both return
   0 when the snapped value falls below the minimum. */
U32 Res_SnapNativeW(U32 dispW);
U32 Res_SnapNativeH(U32 dispH);

/* Compose the visible mode list into `out` (capacity `cap`), newest-first by
   tier: the recommended modes that fit the current display, then a synthesised
   "native" entry derived from the display (deduped), then (when
   includeAdvanced is set) the advanced modes that fit (deduped). 640x480 is always
   emitted at index 0 regardless of display query success. Returns the count
   written (<= cap). Queries the display via Window_GetDisplayDimensions; when
   that fails (headless), only the unconditional 640x480 is emitted. */
int Res_BuildCatalog(ResModeEntry *out, int cap, bool includeAdvanced);

#ifdef __cplusplus
}
#endif
