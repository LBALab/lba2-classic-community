/* Host test for where the authored UI canvas sits in the framebuffer
 * (SOURCES/UI_LAYOUT.H). Header-only: no engine sources to link, runs anywhere
 * with no retail data.
 *
 * The UI's own fixtures are the golden captures in tests/automation, which boot
 * the engine and need retail data and a display, so CI never sees them. What is
 * here is the part it can see: the placement rule itself, checked at every
 * dimension the resolution catalogue can produce rather than at the two sizes a
 * capture happens to run at.
 */
#include "UI_LAYOUT.H"
#include "test_harness.h"

/* The bounds the engine accepts, from RES_CATALOG.H. Repeated rather than
 * included because that header pulls in engine state; if they drift, the sweeps
 * below simply cover a range wider than the engine can reach, which is the safe
 * direction. */
#define RES_MIN_W 320
#define RES_MAX_W 1920
#define RES_MIN_H 200
#define RES_MAX_H 1200

/* Every width the engine can be asked for is a multiple of 8. */
#define W_STEP 8

/* The classic mode is the one thing that must never move: at 640x480 the canvas
 * is the framebuffer, so every offset is 0 and every authored literal lands
 * exactly where it was authored. This is what makes the whole rule adoptable
 * one site at a time. */
static void test_the_classic_mode_is_untouched(void) {
    ASSERT_EQ_INT(0, UiCanvasOriginY(480));
    ASSERT_EQ_INT(0, UiCanvasBottomOfsY(480));
    ASSERT_EQ_INT(320, UiCenterX(640));
}

/* The load-bearing half, and it is vertical only. A negative Y offset is not a UI
 * that hangs off the edge, it is a write through an unclamped row-offset table,
 * so the rule has to hold at every size and not merely at the ones a catalogue
 * lists. X needs no such guarantee because SetClip clamps it, and demanding one
 * would push the canvas centre off a narrow screen. */
static void test_no_vertical_placement_is_ever_negative(void) {
    S32 h;
    S32 bad = 0;

    for (h = 0; h <= RES_MAX_H; h++) {
        if (UiCanvasOriginY(h) < 0 || UiCanvasBottomOfsY(h) < 0)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* The canvas stays inside the framebuffer wherever it fits vertically, so a
 * surface drawn at the origin for the canvas' full height ends on or before the
 * last row. Below the canvas it cannot, and the test says so rather than
 * pretending: the documented answer there is to clip at the bottom. */
static void test_the_canvas_fits_vertically_wherever_it_can(void) {
    S32 h;
    S32 bad = 0;

    for (h = UI_CANVAS_H; h <= RES_MAX_H; h++) {
        if (UiCanvasOriginY(h) + UI_CANVAS_H > h)
            bad++;
        if (UiCanvasBottomOfsY(h) + UI_CANVAS_H > h)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* What the conversion rests on: the named anchor and the ModeDesiredX / 2 the
 * sites spell inline are the same number at every width, narrow ones included.
 * If this fails, converting a site is a visible change rather than a rename, and
 * the commit that does it is no longer behaviour-preserving.
 *
 * The narrow widths are in the sweep deliberately. Anchoring the canvas instead
 * of the framebuffer was tried here, on the theory that a panel reaching 310
 * left of centre wants clamping at 320 wide; it puts the canvas centre at 320,
 * off the right edge, and the menu text with it. */
static void test_center_matches_the_inline_spelling_at_every_legal_width(void) {
    S32 w;
    S32 bad = 0;

    for (w = RES_MIN_W; w <= RES_MAX_W; w += W_STEP) {
        if (UiCenterX(w) != w / 2)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

/* Odd widths agree too, so the conversion cannot shift a site even if the
 * multiple-of-8 rule the catalogue enforces is ever relaxed. */
static void test_odd_widths_agree_as_well(void) {
    ASSERT_EQ_INT(320, UiCenterX(641));
    ASSERT_EQ_INT(324, UiCenterX(649));
}

/* Below the authored width a fixed-width panel runs off both edges, and the left
 * edge goes negative. That is the drawing layer's problem, not this header's:
 * SetClip clamps x to ClipWindowXMin, so the panel is clipped rather than written
 * outside the framebuffer. Pinned here because the numbers look alarming enough
 * to invite a clamp, and a clamp is the wrong answer.
 *
 * The narrow modes are 320x200, 320x240, 400x300 and 512x384 (RES_CATALOG.H). */
static void test_a_wide_panel_hangs_off_both_edges_of_a_narrow_mode(void) {
    /* GAMEMENU's 550-wide highlight bar, centred: -115 to 435 at 320 wide */
    ASSERT_EQ_INT(-115, UiCenterX(320) - 275);
    ASSERT_EQ_INT(435, UiCenterX(320) + 275);

    /* CONFIG's 620-wide keybinding panel: -150 to 470 */
    ASSERT_EQ_INT(-150, UiCenterX(320) - 310);
    ASSERT_EQ_INT(470, UiCenterX(320) + 310);

    /* and it is centred, which is what keeps the middle of the panel on screen */
    ASSERT_EQ_INT(160, UiCenterX(320));
    ASSERT_EQ_INT(200, UiCenterX(400));
    ASSERT_EQ_INT(256, UiCenterX(512));
}

/* Short framebuffers were already clamped before this header existed; the point
 * of the sweep is that moving the expression did not change any answer it gave. */
static void test_short_framebuffers_fall_back_to_the_canvas_top(void) {
    S32 h;
    S32 bad = 0;

    for (h = 0; h < UI_CANVAS_H; h++) {
        if (UiCanvasOriginY(h) != 0 || UiCanvasBottomOfsY(h) != 0)
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
    ASSERT_EQ_INT(0, UiCanvasOriginY(200));
    ASSERT_EQ_INT(0, UiCanvasOriginY(240));
}

/* Tall and wide framebuffers centre. 1920x1080: 300 rows spare each side. */
static void test_large_framebuffers_centre_the_canvas(void) {
    ASSERT_EQ_INT(300, UiCanvasOriginY(1080));
    ASSERT_EQ_INT(600, UiCanvasBottomOfsY(1080));
    ASSERT_EQ_INT(960, UiCenterX(1920));

    /* the shipped widescreen mode, where the canvas is as tall as the framebuffer */
    ASSERT_EQ_INT(0, UiCanvasOriginY(480));
    ASSERT_EQ_INT(384, UiCenterX(768));
}

/* Centring is monotonic: a bigger framebuffer never moves the UI left or up. A
 * non-monotonic step would mean a resolution switch could walk the UI backwards. */
static void test_placement_never_moves_backwards(void) {
    S32 d;
    S32 bad = 0;

    for (d = 1; d <= RES_MAX_W; d++) {
        if (UiCenterX(d) < UiCenterX(d - 1))
            bad++;
    }
    for (d = 1; d <= RES_MAX_H; d++) {
        if (UiCanvasOriginY(d) < UiCanvasOriginY(d - 1))
            bad++;
        if (UiCanvasBottomOfsY(d) < UiCanvasBottomOfsY(d - 1))
            bad++;
    }
    ASSERT_EQ_INT(0, bad);
}

int main(void) {
    RUN_TEST(test_the_classic_mode_is_untouched);
    RUN_TEST(test_no_vertical_placement_is_ever_negative);
    RUN_TEST(test_the_canvas_fits_vertically_wherever_it_can);
    RUN_TEST(test_center_matches_the_inline_spelling_at_every_legal_width);
    RUN_TEST(test_odd_widths_agree_as_well);
    RUN_TEST(test_a_wide_panel_hangs_off_both_edges_of_a_narrow_mode);
    RUN_TEST(test_short_framebuffers_fall_back_to_the_canvas_top);
    RUN_TEST(test_large_framebuffers_centre_the_canvas);
    RUN_TEST(test_placement_never_moves_backwards);
    TEST_SUMMARY();
    return test_failures != 0;
}
