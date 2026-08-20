#include <SYSTEM/INPUT_FLOW.H>

#include <string.h>

//***************************************************************************
T_INPUT_FLOW InputFlow;

/* Previous samples, one per boundary that needs one.
 *
 * Two separate TabKeys snapshots rather than one, because the two boundaries
 * ask about different moments. Boundary 1 wants the state SDL produced, taken
 * before the touch overlay and the harness put their keys in, or an injected
 * key reads as a press the event side lost. Boundary 2 wants the table GetInput
 * really walked, injection included, because a bit that raises no action is the
 * thing it is looking for however the bit got there. */
static U8 PrevSampled[TABKEYS_NUM_KEYS];
static U8 PrevFinal[TABKEYS_NUM_KEYS];
static U32 PrevInput;

static S32 SampledPrimed = FALSE;
static S32 FinalPrimed = FALSE;
static S32 RebuiltPrimed = FALSE;

//***************************************************************************
void InputFlow_NoteKeyEvent(S32 down) {
    if (down)
        InputFlow.KeyDownEvents++;
    else
        InputFlow.KeyUpEvents++;
}

//***************************************************************************
void InputFlow_NotePollSampled(const U8 *tabkeys) {
    /* The first sample has nothing to compare against, and counting every key
       held at that moment as a rise would put a burst at the top of every run
       that no press produced. */
    if (SampledPrimed) {
        InputFlow.PolledRises += InputFlow_CountRises(PrevSampled, tabkeys, TABKEYS_NUM_KEYS);
        InputFlow.PollsSampled++;
    }

    memcpy(PrevSampled, tabkeys, TABKEYS_NUM_KEYS);
    SampledPrimed = TRUE;
}

//***************************************************************************
void InputFlow_NotePollFinal(const U8 *tabkeys) {
    if (FinalPrimed)
        InputFlow.TabKeyRises += InputFlow_CountRises(PrevFinal, tabkeys, TABKEYS_NUM_KEYS);

    memcpy(PrevFinal, tabkeys, TABKEYS_NUM_KEYS);
    FinalPrimed = TRUE;
}

//***************************************************************************
void InputFlow_NoteInputRebuilt(U32 input, U32 masked) {
    if (RebuiltPrimed) {
        InputFlow.InputRises += InputFlow_CountBitRises(PrevInput, input);
        /* What suppression withheld this rebuild. Counted apart from the rest
           because it is the edge filter doing its job, not a boundary losing
           anything, and a reader comparing the two columns needs it excluded. */
        InputFlow.InputRisesMasked += InputFlow_CountBitRises(0, masked);
    }

    PrevInput = input;
    RebuiltPrimed = TRUE;
}

//***************************************************************************
void InputFlow_NoteSimFrame(U32 edge, U32 actionEdgeMask, S32 stepped) {
    if (!edge)
        return;

    InputFlow.FramesWithEdge++;
    if (stepped)
        return;

    InputFlow.FramesWithEdgeSkipped++;
    /* Split out because the two are not the same finding. A skipped movement
       edge is the throttle working as designed: held state the simulation is
       correct to miss. A skipped action edge is the class that produced the
       blowgun that would not stop firing, and the force-step above this call
       exists to make this column zero. */
    if (edge & actionEdgeMask)
        InputFlow.ActionEdgesSkipped++;
}

//***************************************************************************
void InputFlow_Reset(void) {
    memset(&InputFlow, 0, sizeof(InputFlow));
    SampledPrimed = FALSE;
    FinalPrimed = FALSE;
    RebuiltPrimed = FALSE;
    PrevInput = 0;
}

//***************************************************************************
