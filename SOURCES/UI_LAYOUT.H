#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <SYSTEM/ADELINE_TYPES.H>

/* Where the authored UI canvas sits inside the framebuffer, in one place and
 * free of engine state.
 *
 * The 2D UI was authored against a 640x480 4:3 canvas and still is: every menu
 * coordinate, panel width and text anchor in the game is a literal measured
 * against that canvas. The framebuffer underneath it is whatever the player
 * chose, from 320x200 to 1920x1080, and reconciling the two is this header.
 *
 * The two axes do not answer it the same way, which is the thing to understand
 * before using either:
 *
 *   X centres on the framebuffer at every size. A surface too wide to fit is
 *   drawn centred and clipped at both edges, which keeps its middle (where the
 *   text is) on screen.
 *
 *   Y centres on the framebuffer only where the canvas fits, and falls back to
 *   the canvas-top origin below that, never to a negative offset.
 *
 * The asymmetry is not an oversight. A negative Y reaches the row-offset lookup
 * (TabOffLine[], indexed unclamped by BackupAngles and the pixel-shift macros)
 * and indexes it out of bounds, so it is a write outside the framebuffer rather
 * than a draw off the edge of it. A negative X has no equivalent: it goes
 * through SetClip, which clamps it to ClipWindowXMin. Mirroring the Y clamp onto
 * X was tried and is wrong: it anchors the canvas at 0 and puts its centre at
 * 320, which is off the right edge of a 320-wide framebuffer, so the menu text
 * leaves the screen.
 *
 * Reading the live dimensions is deliberate: never cache what these return, or
 * the value goes stale after a runtime resolution switch (Res_Switch).
 *
 * The host test in tests/ui_layout/test_ui_layout.cpp checks both axes over
 * every dimension the resolution catalogue can produce.
 */

/* The height of the canvas the 2D UI is authored against. Not a resolution: the
 * framebuffer is RESOLUTION_X/Y (SYSTEM/RESOLUTION.H) and this is the coordinate
 * space the UI literals live in. They differ whenever the player is not at
 * 640x480, which is the whole reason this header exists.
 *
 * Most X placement centres on the framebuffer at every size, so the authored
 * width never enters that arithmetic. The surfaces that are laid out as a whole
 * 4:3 block rather than centred per element (the inventory, the letterboxed
 * dialogue box) do need it, which is what UI_CANVAS_W and UiCanvasOriginX are
 * for. */
#define UI_CANVAS_H 480
#define UI_CANVAS_W 640

/* Where the canvas starts inside a framebuffer `modeH` tall, and the same pinned
 * to the bottom edge for the surfaces authored against it (the dialogue box, the
 * inventory bar).
 *
 * Clamped to >= 0 for the reason in the header comment above: below the canvas
 * the authored UI does not fit, and the choice is between clipping it at the
 * bottom (origin 0) and indexing the row table out of bounds. */
static inline S32 UiCanvasOriginY(S32 modeH) {
    return modeH > UI_CANVAS_H ? (modeH - UI_CANVAS_H) / 2 : 0;
}

/* X twin of UiCanvasOriginY, and clamped for the same reason. Below the canvas
 * width the authored block does not fit, and a negative origin puts its left
 * edge off-screen, which is how the frame corner copies in INVENT.CPP came to
 * index outside Log at 320 wide. Clipping at the left edge instead keeps the
 * visible part on screen and the arithmetic in bounds. */
static inline S32 UiCanvasOriginX(S32 modeW) {
    return modeW > UI_CANVAS_W ? (modeW - UI_CANVAS_W) / 2 : 0;
}

static inline S32 UiCanvasBottomOfsY(S32 modeH) {
    return modeH > UI_CANVAS_H ? modeH - UI_CANVAS_H : 0;
}

/* The horizontal anchor: the centre of the framebuffer, which is what nearly
 * every 2D site asks for, either as a centred width or as an offset from it.
 *
 * This is what those sites already compute inline, and naming it is the whole
 * change: a reader can no longer mistake it for the row stride or the iso
 * projection origin, which are the other two things ModeDesiredX is used for. */
static inline S32 UiCenterX(S32 modeW) {
    return modeW / 2;
}

/* The live forms, bound to the framebuffer dimensions the engine is running at
 * (ModeDesiredX / ModeDesiredY, SVGA/SCREEN.H). Macros rather than functions so
 * this header stays free of engine state and can be host-tested; a caller needs
 * SCREEN.H in scope, which every UI translation unit already has.
 *
 * UI_VCENTER_OFS and UI_VBOTTOM_OFS keep the names they were introduced with,
 * so the sites that already use them are unchanged. */
#define UI_VCENTER_OFS UiCanvasOriginY((S32)ModeDesiredY)
#define UI_VBOTTOM_OFS UiCanvasBottomOfsY((S32)ModeDesiredY)
#define UI_CENTER_X UiCenterX((S32)ModeDesiredX)
#define UI_HCANVAS_OFS UiCanvasOriginX((S32)ModeDesiredX)

#endif /* UI_LAYOUT_H */
