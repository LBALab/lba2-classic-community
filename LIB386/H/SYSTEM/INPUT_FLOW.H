#ifndef LIB_SYSTEM_INPUT_FLOW
#define LIB_SYSTEM_INPUT_FLOW

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD.H> /* TABKEYS_NUM_KEYS */

/* Counts the real input signal on both sides of each layer it crosses, and
 * reports where the two sides disagree.
 *
 * Input crosses three boundaries and each can lose a press for its own reason.
 * Every one of those losses is invisible from inside the layer that dropped it:
 * the layer below never had the signal, and the layer above cannot tell a press
 * that never happened from one that did and was eaten.
 *
 *   1. SDL event to polled state. The keyboard is read as level state once a
 *      frame, not as a queue, so a press and release that both land between two
 *      polls are never sampled. Counted as key-down events against rising edges
 *      in TabKeys.
 *
 *   2. Polled state to action bits. A key that no binding names raises nothing,
 *      which includes the four spell slots that deliberately never reach Input.
 *      Counted as TabKeys rising edges against Input rising bits.
 *
 *   3. Action bits to the simulation. The fixed-timestep throttle skips whole
 *      rendered frames, and an edge landing on a skipped one is never seen by
 *      the object loop. Counted as frames carrying an Input edge against frames
 *      the simulation actually stepped.
 *
 * This is not `input trace`, which logs the mask the harness injected and the
 * hero state per sim tick. That serves the simulator. This counts the signal
 * that really arrived at each layer and reports the disagreement, which is the
 * only thing that can say whether a boundary loses anything in practice.
 *
 * Read-only by construction: nothing here is consulted by the engine, and every
 * entry point below only adds. `DbgHeroActionResets`, reported as
 * `action_resets`, is the same idea applied at one consumer rather than at the
 * boundaries.
 *
 * The arithmetic is separated from the accumulation on purpose: the counting
 * rules are pure functions of two samples and are covered by tests/input_flow
 * with no engine underneath, the way the camera's angle arithmetic is.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* --- boundary 1: SDL event to polled state --------------------------- */
    U32 KeyDownEvents; /* SDL_EVENT_KEY_DOWN, auto-repeat excluded */
    U32 KeyUpEvents;   /* SDL_EVENT_KEY_UP */
    U32 PolledRises;   /* 0 -> 1 in the SDL-sampled half of TabKeys */
    U32 PollsSampled;  /* denominator: how many polls the above covers */

    /* --- boundary 2: polled state to action bits ------------------------- */
    U32 TabKeyRises;      /* 0 -> 1 anywhere in TabKeys, injection included */
    U32 InputRises;       /* 0 -> 1 in Input */
    U32 InputRisesMasked; /* rises NoRepeatInput deliberately withheld */

    /* --- boundary 3: action bits to the simulation ----------------------- */
    U32 FramesWithEdge;        /* rendered frames where Input changed */
    U32 FramesWithEdgeSkipped; /* of those, frames the throttle skipped */
    U32 ActionEdgesSkipped;    /* of those, ones carrying an I_ACTION_EDGE bit */
} T_INPUT_FLOW;

extern T_INPUT_FLOW InputFlow;

/* --- the counting rules, as pure functions ------------------------------- */

/* How many bits went 0 -> 1 between two byte-array samples of the same size.
 * TabKeys is a bitmap in two halves (one bit per key above 255, the whole byte
 * below it), and a rise is a rise in either, so this counts set bits of
 * `cur & ~prev` rather than comparing bytes for inequality: a byte holding two
 * keys that both rose is two presses, not one. */
static inline U32 InputFlow_CountRises(const U8 *prev, const U8 *cur, S32 bytes) {
    U32 rises = 0;
    S32 i;

    for (i = 0; i < bytes; i++) {
        U8 gained = (U8)(cur[i] & ~prev[i]);
        while (gained) {
            rises += (gained & 1u);
            gained = (U8)(gained >> 1);
        }
    }
    return rises;
}

/* The same question for the Input word. */
static inline U32 InputFlow_CountBitRises(U32 prev, U32 cur) {
    U32 gained = cur & ~prev;
    U32 rises = 0;

    while (gained) {
        rises += (gained & 1u);
        gained >>= 1;
    }
    return rises;
}

/* --- entry points, all of them additive ---------------------------------- */

/* Boundary 1, event side. Auto-repeat is the caller's to exclude: a held key
 * repeats at the desktop's rate and none of those are presses. */
extern void InputFlow_NoteKeyEvent(S32 down);

/* Boundary 1, state side. Called with the SDL-sampled state only, before any
 * virtual or harness key is applied, so an injected key cannot look like a
 * press the event side missed. */
extern void InputFlow_NotePollSampled(const U8 *tabkeys);

/* Boundary 2, key side. Called with the table as GetInput will walk it, after
 * the virtual and harness hooks have put their keys in, because a bit that
 * raises no action is what this boundary looks for however the bit got there.
 *
 * Deliberately taken here rather than inside GetInput, which reaches the table
 * only through CheckKey and should keep it that way: making the funnel name
 * TabKeys would couple it to the keyboard module and force every test of it to
 * supply one. */
extern void InputFlow_NotePollFinal(const U8 *tabkeys);

/* Boundary 2, action side. `masked` is what NoRepeatInput withheld, which is
 * suppression working rather than signal lost. */
extern void InputFlow_NoteInputRebuilt(U32 input, U32 masked);

/* Boundary 3. Called once per rendered frame with that frame's edge and
 * whether the simulation stepped. */
extern void InputFlow_NoteSimFrame(U32 edge, U32 actionEdgeMask, S32 stepped);

extern void InputFlow_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* LIB_SYSTEM_INPUT_FLOW */
