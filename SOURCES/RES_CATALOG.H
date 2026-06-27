/*
 * Resolution catalog: the shared list of selectable render resolutions and
 * the helpers that compose it. One home for the data and rules that used to
 * live duplicated in the console `resolution` verb (CONSOLE_CMD.CPP) and the
 * Display options submenu (RES_MENU.CPP):
 *
 *   - the canonical (W, H) bounds and the multiple-of-8 width rule,
 *   - aspect-ratio reduction, labelling, and display-aspect / native snapping,
 *   - two list builders: Res_BuildMenuList (the in-game menu's per-monitor
 *     aspect-matched ladder) and Res_BuildCatalog (the console verb's curated
 *     recommended + advanced tables).
 *
 * Consumers layer their own presentation on top: the console prints with tags;
 * the menu formats "WxH (aspect)" rows; RES_SWITCH validates against the same
 * bounds. See docs/RUNTIME_RESOLUTION.md.
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

/* Write a human aspect label for (w, h) into `out`: a standard ratio name
   ("16:9", "4:3", "16:10", ...) when within ~1.5% of one (so a %8-snapped
   widescreen mode reads "16:9", not "53:30"), else the exact reduced ratio. */
void Res_AspectLabel(U32 w, U32 h, char *out, int outSize);

/* Snap a display dimension into the supported range. Width additionally
   rounds down to the nearest multiple of 8 (e.g. 1366 -> 1360). Both return
   0 when the snapped value falls below the minimum. */
U32 Res_SnapNativeW(U32 dispW);
U32 Res_SnapNativeH(U32 dispH);

/* Width that preserves the display aspect at `targetH` render height, rounded
   down to a multiple of 8 and clamped to [RES_MIN_W, RES_MAX_W]. Returns 0 when
   inputs are zero or the result falls outside the width range. The building
   block for the menu's aspect ladder (Res_BuildMenuList). */
U32 Res_SnapAspectWidth(U32 dispW, U32 dispH, U32 targetH);

/* targetH = 480 shorthand: the classic-height widescreen width. Shared by the
   boot auto-detect (Res_AutoDetectDimensions) and the menu's @480 rung, so the
   first-launch resolution is always one the menu lists. */
U32 Res_SnapWidescreenWidth(U32 dispW, U32 dispH);

/* The in-game menu's list: the 640x480 classic anchor plus a ladder of
   display-aspect-matched modes (at 480/720/1080 render heights) that fill this
   exact panel, computed per monitor. Drops rungs that don't fit the display or
   exceed the max width, and dups of the anchor. Ascending order. Returns the
   count written (<= cap); headless yields just the anchor. Distinct from
   Res_BuildCatalog, which is the console verb's curated/advanced list. */
int Res_BuildMenuList(ResModeEntry *out, int cap);

/* The console verb's list (recommended + native, plus advanced when set),
   filtered to the display. A curated table of common modes for power users;
   see Res_BuildMenuList for the per-monitor aspect-matched menu list. 640x480
   is always emitted at index 0. Returns the count written (<= cap). */
int Res_BuildCatalog(ResModeEntry *out, int cap, bool includeAdvanced);

#ifdef __cplusplus
}
#endif
