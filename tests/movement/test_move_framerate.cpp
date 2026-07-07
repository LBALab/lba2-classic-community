/* Root-cause characterization: animation-driven locomotion is frame-rate
 * dependent (issue #358).
 *
 * In LBA2 there is no "walk speed * dt": hero/NPC horizontal displacement is baked
 * into the animation keyframe translations and delivered by ObjectSetInterDep
 * (LIB386/ANIM/INTERDEP.CPP), which the main loop calls exactly ONCE per rendered
 * frame per object (SOURCES/OBJECT.CPP GereObjAnim, no catch-up loop). So the
 * distance walked over a fixed span of *game-time* should not depend on how finely
 * that span is sampled, i.e. on the frame rate. It does.
 *
 * This test drives the real C ObjectSetInterDep over the SAME span of game-time at
 * several dt granularities and asserts the walked distance materially DIFFERS across
 * them, pinning the coupling as an executable, platform-independent fact (the same
 * shape measured end-to-end with the --fixed-dt harness sweep, see
 * docs/MOVEMENT_FRAMERATE.md). It is a characterization, not a bug that flips green:
 * the fix (the main-loop throttle) works around this at the loop level without changing
 * this function; its logic is gated by tests/timer/test_fixed_step.cpp.
 *
 * The dominant effect here is the LOW-frame-rate overshoot-discard: when a keyframe
 * completes, ObjectSetInterDep re-anchors the next window to obj->Time
 * (NextTimer = obj->Time + dur, INTERDEP.CPP), throwing away the overshoot past the
 * keyframe's intended end, so the animation falls behind real game-time. With a
 * 50 ms keyframe the distance is ideal at high frame rates and plateaus ~22% short
 * from ~60 fps down. (The opposing HIGH-frame-rate rounding loss needs sub-unit
 * per-frame deltas and is angle-dependent; it is characterized against the real
 * game in the doc rather than asserted here.)
 *
 * CPP-only: drives the real C ObjectSetInterDep, links the real 3D math. No ASM,
 * no Docker, no game assets. Runs in host_quick CI.
 */
#include "test_harness.h"
#include <ANIM/INTERDEP.H>
#include <ANIM/ANIM.H>
#include <ANIM/LIBINIT.H>
#include <ANIM/CLEAR.H>
#include <ANIM.H>
#include <OBJECT/AFF_OBJ.H>
#include <math.h>

#include "anim_test_fixture.h"

extern "C" U32 TimerRefHR;

/* Total span of game-time each run integrates over (ms). */
#define WALK_MS 3200
/* Frame-rate-independent target: 3200 ms / (4 keyframes * 50 ms) = 16 cycles,
 * 4 * 300 units of forward Z per cycle. */
#define WALK_IDEAL (16 * 4 * 300)

static U8 g_walk_anim[512];

/* A looping "walk": 4 keyframes, 50 ms each, a fixed forward Z step per frame.
 * Translation lives in the frame-header slots [1..3] (X,Y,Z) that ObjectSetInterDep
 * reads; Master stays 0 so there is no angle animation and the object keeps facing
 * whatever Beta we point it at. */
static void build_walk_anim(void) {
    build_anim_header(g_walk_anim, /*nbFrames*/ 4, /*nbGroups*/ 2,
                      /*loopFrame*/ 0, /*deltaTime*/ 50);
    for (U16 f = 0; f < 4; f++) {
        S16 *fp = anim_frame_ptr(g_walk_anim, 2, f);
        fp[1] = 0;   /* X translation */
        fp[2] = 0;   /* Y translation (override the fixture's NextBody = -1) */
        fp[3] = 300; /* Z translation: a forward step per keyframe */
    }
}

/* Walk for WALK_MS of game-time in dt-ms steps facing `beta`; return the net world
 * displacement magnitude in the XZ plane. */
static double run_walk(U16 dt, S32 beta) {
    build_walk_anim();

    T_OBJ_3D obj;
    init_test_obj(&obj);
    init_anim_buffer();
    TransFctAnim = NULL;
    /* Anchor the clock at 0 BEFORE ObjectInitAnim: it seeds obj->Time from
     * TimerRefHR, and a stale (larger) value would send the first step down
     * ObjectSetInterDep's rewind branch and corrupt the timers. */
    TimerRefHR = 0;
    ObjectInitAnim(&obj, g_walk_anim);
    obj.Status = FLAG_CHANGE;
    obj.LastFrame = 0;
    obj.NextFrame = 1;
    obj.LastOfsFrame = (PTR_U32)anim_frame_offset(2, 0);
    obj.NextOfsFrame = (PTR_U32)anim_frame_offset(2, 1);
    obj.LastOfsIsPtr = 0;
    obj.LastTimer = 0;
    obj.NextTimer = 50;
    obj.Alpha = 0;
    obj.Beta = beta;
    obj.Gamma = 0;
    obj.X = 0;
    obj.Y = 0;
    obj.Z = 0;

    for (U32 t = dt; t <= WALK_MS; t += dt) {
        TimerRefHR = t;
        ObjectSetInterDep(&obj);
    }
    return sqrt((double)obj.X * obj.X + (double)obj.Z * obj.Z);
}

/* The property under test: same game-time walks the same distance regardless of the
 * frame rate it is sampled at. Beta=0 isolates the low-frame-rate overshoot-discard
 * (no rotation rounding), so the failure is clean and deterministic. */
static void test_framerate_coupling_present(void) {
    const int fps_dt[] = {1, 2, 4, 8, 16, 32, 64};
    double d[7], lo = 1e18, hi = 0.0;
    printf("  walked over %d ms of game-time (ideal = %d):\n", WALK_MS, WALK_IDEAL);
    for (int i = 0; i < 7; i++) {
        d[i] = run_walk((U16)fps_dt[i], 0);
        if (d[i] < lo)
            lo = d[i];
        if (d[i] > hi)
            hi = d[i];
        printf("    ~%4d fps (dt=%2d): %.0f  (%.0f%% of ideal)\n",
               1000 / fps_dt[i], fps_dt[i], d[i], 100.0 * d[i] / WALK_IDEAL);
    }
    /* Characterization: the same game-time walks a materially DIFFERENT distance at
     * different dt (here ~22%: 100% of ideal at dt=1 down to a 78% plateau from dt=16).
     * That is the root-cause coupling issue #358 is about. The fix does not change this
     * function (that would diverge from the ASM oracle); it is the main-loop throttle that
     * pins the effective sim dt near 16 ms, whose logic is gated by
     * tests/timer/test_fixed_step.cpp. If this coupling ever disappears here, the throttle's
     * premise changed -- revisit the fix. */
    ASSERT_TRUE((hi - lo) > 0.1 * hi);
}

int main(void) {
    RUN_TEST(test_framerate_coupling_present);
    TEST_SUMMARY();
    return test_failures != 0;
}
