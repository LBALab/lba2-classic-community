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
 * several dt granularities and prints/asserts the walked distance. It FAILS on the
 * current engine (that is the bug) and pins the coupling as an executable,
 * platform-independent fact, the same shape measured end-to-end with the
 * --fixed-dt harness sweep (see docs/MOVEMENT_FRAMERATE.md).
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
 * NOTE ON THE FIX LEVEL: the chosen fix is a fixed simulation timestep in the main
 * loop, which pins this function to a constant dt in production rather than changing
 * its arithmetic (that would diverge from the ASM oracle the equivalence tests hold
 * it to). So this unit-level invariance is not itself the merge gate; it is the
 * characterization. The end-to-end gate is the render-rate sweep in
 * tests/automation/test_move_framerate.sh.
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
/* Walked distance must match across frame rates within this fraction. */
#define WALK_TOL 0.02

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
static void test_framerate_invariance(void) {
    const int fps_dt[] = {1, 2, 4, 8, 16, 32, 64};
    double d[7];
    printf("  walked over %d ms of game-time (ideal = %d):\n", WALK_MS, WALK_IDEAL);
    for (int i = 0; i < 7; i++) {
        d[i] = run_walk((U16)fps_dt[i], 0);
        printf("    ~%4d fps (dt=%2d): %.0f  (%.0f%% of ideal)\n",
               1000 / fps_dt[i], fps_dt[i], d[i], 100.0 * d[i] / WALK_IDEAL);
    }
    /* High frame rate (dt=1) vs 60 fps (dt=16) vs low frame rate (dt=64): all
     * should walk the same distance. They do not today. */
    double ref = d[4];                               /* dt=16, ~60 fps */
    ASSERT_TRUE(fabs(d[0] - ref) <= WALK_TOL * ref); /* dt=1  vs dt=16 */
    ASSERT_TRUE(fabs(d[6] - ref) <= WALK_TOL * ref); /* dt=64 vs dt=16 */
}

int main(void) {
    RUN_TEST(test_framerate_invariance);
    TEST_SUMMARY();
    return test_failures != 0;
}
