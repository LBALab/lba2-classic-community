/* Host test: the per-boundary input flow counters.
 *
 * The counters exist to answer, with a number rather than a grep, whether each
 * layer input crosses loses anything. That only means something if the counting
 * rules themselves are right, and every one of them is a pure function of two
 * samples, so they can be driven here with no SDL, no keyboard and no engine.
 *
 * What the rules have to get right, in the order a frame meets them:
 *
 *   - A rise is a bit going 0 to 1, not a byte changing. TabKeys packs the keys
 *     above 255 eight to a byte, so two pad buttons pressed in the same frame
 *     are two presses and a byte comparison would call them one.
 *
 *   - The first sample of a run establishes a baseline and counts nothing. Any
 *     key already down when counting starts is not a press, and reporting it as
 *     one would put a burst at the top of every measurement.
 *
 *   - What NoRepeatInput withheld is counted apart from what arrived. It is
 *     suppression working, and a reader comparing the two sides of boundary 2
 *     needs it out of the way.
 *
 *   - A frame with no input change is not a frame the simulation missed, and a
 *     skipped movement edge is not the same finding as a skipped action edge.
 *     The throttle is designed to skip the first.
 */
#include <cstdio>
#include <cstring>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/INPUT_FLOW.H>

static int fails = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (!(cond)) {                                      \
            std::fprintf(stderr, "%s:%d: FAIL: ", __FILE__, \
                         __LINE__);                         \
            std::fprintf(stderr, __VA_ARGS__);              \
            std::fprintf(stderr, "\n");                     \
            fails++;                                        \
        }                                                   \
    } while (0)

namespace {

// A whole-table sample, so the tests speak in the same units the engine does.
struct Keys {
    U8 b[TABKEYS_NUM_KEYS];
    Keys() { std::memset(b, 0, sizeof b); }
    // Below 256 a key owns a whole byte; above it, one bit of one byte.
    void down(U32 key) {
        if (key < 256)
            b[key] |= 0x80;
        else
            b[256 - 32 + (key >> 3)] |= (U8)(1 << (key & 7));
    }
    void up(U32 key) {
        if (key < 256)
            b[key] &= (U8)~0x80;
        else
            b[256 - 32 + (key >> 3)] &= (U8) ~(1 << (key & 7));
    }
};

// --- 1. A rise is a bit, not a byte -----------------------------------------
void CheckRisesCountBits(void) {
    Keys a, b;
    CHECK(InputFlow_CountRises(a.b, b.b, TABKEYS_NUM_KEYS) == 0, "nothing held, no rises");

    b.down(30);
    CHECK(InputFlow_CountRises(a.b, b.b, TABKEYS_NUM_KEYS) == 1, "one key down is one rise");

    // Two keys that share a byte in the packed half. A byte comparison would
    // report one press here, which is the bug this rule exists to avoid.
    Keys c, d;
    c.down(1024);
    d.down(1024);
    d.down(1025);
    d.down(1026);
    CHECK(InputFlow_CountRises(c.b, d.b, TABKEYS_NUM_KEYS) == 2,
          "two more bits in an occupied byte are two rises, got %u",
          InputFlow_CountRises(c.b, d.b, TABKEYS_NUM_KEYS));

    // A release is not a rise, and neither is holding still.
    CHECK(InputFlow_CountRises(d.b, c.b, TABKEYS_NUM_KEYS) == 0, "releases are not rises");
    CHECK(InputFlow_CountRises(d.b, d.b, TABKEYS_NUM_KEYS) == 0, "a held key is not a new press");

    CHECK(InputFlow_CountBitRises(0x0, 0x5) == 2, "two Input bits rose");
    CHECK(InputFlow_CountBitRises(0x5, 0x5) == 0, "held Input bits are not new");
    CHECK(InputFlow_CountBitRises(0x5, 0x1) == 0, "a cleared bit is not a rise");
    CHECK(InputFlow_CountBitRises(0u, 0x80000000u) == 1, "the top bit counts");
}

// --- 2. Boundary 1: the first sample is a baseline ---------------------------
void CheckFirstSampleIsBaseline(void) {
    InputFlow_Reset();

    Keys held;
    held.down(30); // already down when counting starts
    InputFlow_NotePollSampled(held.b);
    CHECK(InputFlow.PolledRises == 0, "a key already down at the first poll is not a press");
    CHECK(InputFlow.PollsSampled == 0, "and that poll is not a denominator either");

    InputFlow_NotePollSampled(held.b); // still held
    CHECK(InputFlow.PolledRises == 0, "still held, still not a press");
    CHECK(InputFlow.PollsSampled == 1, "the second poll is measurable");

    Keys more(held);
    more.down(31);
    InputFlow_NotePollSampled(more.b);
    CHECK(InputFlow.PolledRises == 1, "a new key down is one rise, got %u", InputFlow.PolledRises);
}

// --- 3. Boundary 1: the loss it exists to find ------------------------------
// A press and release that both land between two polls. The events see two, the
// polled state sees nothing, and the gap is the whole point of the boundary.
void CheckPressBetweenPollsIsVisibleAsAGap(void) {
    InputFlow_Reset();

    Keys idle;
    InputFlow_NotePollSampled(idle.b); // prime
    InputFlow_NoteKeyEvent(TRUE);      // pressed...
    InputFlow_NoteKeyEvent(FALSE);     // ...and released, both before the next poll
    InputFlow_NotePollSampled(idle.b);

    CHECK(InputFlow.KeyDownEvents == 1, "the event side saw the press");
    CHECK(InputFlow.KeyUpEvents == 1, "and the release");
    CHECK(InputFlow.PolledRises == 0, "the polled side saw nothing");
    CHECK(InputFlow.KeyDownEvents - InputFlow.PolledRises == 1,
          "the gap is one lost press, which is what this boundary reports");
}

// --- 4. Boundary 2: suppression is counted apart ----------------------------
void CheckMaskedRisesAreSeparate(void) {
    InputFlow_Reset();

    InputFlow_NoteInputRebuilt(0, 0); // prime
    InputFlow_NoteInputRebuilt(0x3, 0);
    CHECK(InputFlow.InputRises == 2, "two action bits rose");
    CHECK(InputFlow.InputRisesMasked == 0, "nothing was withheld");

    // The edge filter holds one bit back this rebuild.
    InputFlow_NoteInputRebuilt(0x3, 0x1);
    CHECK(InputFlow.InputRises == 2, "a withheld bit did not rise again");
    CHECK(InputFlow.InputRisesMasked == 1,
          "and it is reported as suppression, not as loss, got %u",
          InputFlow.InputRisesMasked);
}

// --- 5. Boundary 2: a key that names no action ------------------------------
void CheckUnboundKeyShowsAsAGap(void) {
    InputFlow_Reset();

    Keys none, spell;
    spell.down(45); // a spell slot: bound in the table, never folded into Input

    InputFlow_NotePollFinal(none.b); // prime
    InputFlow_NoteInputRebuilt(0, 0);

    InputFlow_NotePollFinal(spell.b);
    InputFlow_NoteInputRebuilt(0, 0); // no action bit rose

    CHECK(InputFlow.TabKeyRises == 1, "the key went down");
    CHECK(InputFlow.InputRises == 0, "and raised no action");
    CHECK(InputFlow.TabKeyRises - InputFlow.InputRises == 1,
          "which is the gap boundary 2 exists to report");
}

// --- 6. Boundary 3: what the simulation missed ------------------------------
void CheckSimFrameSplit(void) {
    const U32 kActionEdge = 0x00000010; // stands in for I_ACTION_EDGE
    const U32 kMove = 0x00000001;       // a movement bit, deliberately skippable

    InputFlow_Reset();

    InputFlow_NoteSimFrame(0, kActionEdge, FALSE);
    CHECK(InputFlow.FramesWithEdge == 0, "a frame with no input change is not a miss");

    InputFlow_NoteSimFrame(kMove, kActionEdge, TRUE);
    CHECK(InputFlow.FramesWithEdge == 1, "a stepped frame with an edge is counted");
    CHECK(InputFlow.FramesWithEdgeSkipped == 0, "and it was not skipped");

    InputFlow_NoteSimFrame(kMove, kActionEdge, FALSE);
    CHECK(InputFlow.FramesWithEdgeSkipped == 1, "a skipped movement edge is counted");
    CHECK(InputFlow.ActionEdgesSkipped == 0,
          "but not as an action edge: the throttle is designed to skip held movement");

    InputFlow_NoteSimFrame(kActionEdge, kActionEdge, FALSE);
    CHECK(InputFlow.FramesWithEdgeSkipped == 2, "a skipped action edge is a skipped frame");
    CHECK(InputFlow.ActionEdgesSkipped == 1,
          "and is reported apart, because the force-step exists to keep this at zero");
}

// --- 7. Reset clears everything ---------------------------------------------
// Including the primed flags: a second measurement that inherited the first
// one's last sample would open with a burst of rises nobody pressed.
void CheckResetClearsBaselines(void) {
    Keys held;
    held.down(30);
    InputFlow_NotePollSampled(held.b);
    InputFlow_NotePollSampled(held.b);
    InputFlow_NoteKeyEvent(TRUE);

    InputFlow_Reset();
    CHECK(InputFlow.KeyDownEvents == 0 && InputFlow.PolledRises == 0 && InputFlow.PollsSampled == 0,
          "the counters are clear");

    /* The baseline has to be clear too, and the counters alone cannot say so:
       the next sample must be treated as a new baseline rather than measured
       against the sample from before the reset. PollsSampled is what
       distinguishes them, because a baseline is not a denominator. */
    InputFlow_NotePollSampled(held.b);
    CHECK(InputFlow.PollsSampled == 0,
          "the first sample after a reset is a baseline, not a measurement (polls=%u)",
          InputFlow.PollsSampled);

    Keys more(held);
    more.down(31);
    InputFlow_NotePollSampled(more.b);
    CHECK(InputFlow.PollsSampled == 1 && InputFlow.PolledRises == 1,
          "counting resumes from that fresh baseline, got %u rise(s) over %u poll(s)",
          InputFlow.PolledRises, InputFlow.PollsSampled);
}

} // namespace

int main() {
    CheckRisesCountBits();
    CheckFirstSampleIsBaseline();
    CheckPressBetweenPollsIsVisibleAsAGap();
    CheckMaskedRisesAreSeparate();
    CheckUnboundKeyShowsAsAGap();
    CheckSimFrameSplit();
    CheckResetClearsBaselines();

    if (fails == 0) {
        std::printf("test_input_flow: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_flow: %d failure(s)\n", fails);
    return 1;
}
