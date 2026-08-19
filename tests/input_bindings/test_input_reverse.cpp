/* Host test: reading the binding tables backwards.
 *
 * InitInput folds DefKeys and GamepadKeys into the flat (key, mask) array
 * GetInput scans, which answers "is this slot held" and nothing else. The
 * reverse -- which keys carry a slot, which slot a key carries -- is done by
 * hand wherever it is needed today, and docs/plan/RECORDING_RESEARCH.md prices
 * it as the piece an action-shaped recording would rest on.
 *
 * What this pins:
 *
 *   - The round trip. For every slot the retail layout binds, a key the reverse
 *     hands back is a key the forward fold raises that slot's bit for, and no
 *     other slot's. That is the property anything re-expressing an action as a
 *     key press depends on, and it is a claim about the two directions agreeing
 *     rather than about either one alone.
 *
 *   - The spell slots. The combined table stops at 32 because the four spells
 *     have no Input bit to fold into, and the reverse deliberately does not
 *     inherit that ceiling: PERSO.CPP polls 32-35 with the same OR of four keys
 *     through SpellKeyDown, so a lookup that stopped at 32 would report the
 *     penguin as unbound.
 *
 *   - The two ways a table stops being reversible, which the forward direction
 *     does not care about and a cfg does not prevent: one key on two slots, and
 *     a slot with no key at all. Both are reported rather than papered over,
 *     because a reverse that guesses produces a frame the recording never had.
 *
 * The retail layout satisfies the round trip, which test_input_bindings already
 * relies on when it probes one key at a time. This test says so as a property
 * of the tables rather than as an assumption of a probe.
 */
#include <cstdio>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/INPUT.H>         // Input, GetInput, ClearNoRepeatInput
#include <SYSTEM/KEYBOARD_KEYS.H> // K_*

#include "INPUT.H" // T_DEF_KEY, MAX_INPUT_SLOTS, InitInput, RestoreInput, Bindings_*

#include "held_keys.h"

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

// Slots 0-31 fold into Input; 32-35 do not, and are polled the way PERSO.CPP's
// SpellKeyDown polls them.
const S32 kInputSlots = 32;

U32 InputFromHolding(U32 key) {
    HoldOnly(key);
    ClearNoRepeatInput();
    GetInput(0);
    return Input;
}

void Reset() {
    RestoreInput();
    InitInput();
    HoldNone();
}

// Every slot the retail layout binds hands back at least one key, including the
// four the combined table has no room for.
void CheckEveryRetailSlotIsReachable() {
    Reset();
    for (S32 slot = 0; slot < MAX_INPUT_SLOTS; slot++) {
        U32 keys[4];
        S32 n = Bindings_KeysForSlot(slot, keys, 4);
        CHECK(n > 0, "retail slot %d has no key to press", (int)slot);
    }
}

// The round trip, for the slots that reach Input: press what the reverse says
// and the fold raises that slot's bit alone.
void CheckRoundTripThroughTheFold() {
    Reset();
    for (S32 slot = 0; slot < kInputSlots; slot++) {
        U32 keys[4];
        S32 n = Bindings_KeysForSlot(slot, keys, 4);
        for (S32 k = 0; k < n; k++) {
            // The pad half of a slot is a K_GAMEPAD_* code, which reaches the
            // same fold; CheckKey answers for it out of the same table.
            U32 got = InputFromHolding(keys[k]);
            CHECK(got == (1u << slot),
                  "slot %d key %u: fold raised %u, wanted only bit %d",
                  (int)slot, (unsigned)keys[k], (unsigned)got, (int)slot);
        }
    }
}

// And the other direction agrees with itself: a key the reverse hands out for a
// slot names that slot back.
void CheckKeyNamesItsSlot() {
    Reset();
    for (S32 slot = 0; slot < MAX_INPUT_SLOTS; slot++) {
        U32 keys[4];
        S32 n = Bindings_KeysForSlot(slot, keys, 4);
        for (S32 k = 0; k < n; k++)
            CHECK(Bindings_SlotForKey(keys[k]) == slot,
                  "key %u of slot %d named slot %d", (unsigned)keys[k],
                  (int)slot, (int)Bindings_SlotForKey(keys[k]));
    }
}

// Zero is the unbound marker in both tables, not a key. Answering for it would
// make every empty cell a match, and most cells are empty.
void CheckUnboundMarkerIsNotAKey() {
    Reset();
    CHECK(Bindings_SlotForKey(0) == -1, "zero must not name a slot");
    CHECK(Bindings_SlotForKey(K_F12) == -1,
          "a key no slot binds must not name one");
}

void CheckOutOfRangeSlots() {
    Reset();
    U32 keys[4];
    CHECK(Bindings_KeysForSlot(-1, keys, 4) == 0, "slot -1 yields nothing");
    CHECK(Bindings_KeysForSlot(MAX_INPUT_SLOTS, keys, 4) == 0,
          "slot past the table yields nothing");
    CHECK(Bindings_KeysForSlot(0, NULL, 4) == 0, "a null destination yields nothing");
}

// `max` is a cap on what the caller can hold, not a filter on the answer.
void CheckMaxIsRespected() {
    Reset();
    U32 keys[4] = {0, 0, 0, 0};
    // Slot 0 is bound on both halves in the retail layout: two keyboard keys
    // and a pad direction.
    S32 n = Bindings_KeysForSlot(0, keys, 1);
    CHECK(n == 1, "asked for one key, got %d", (int)n);
    CHECK(keys[0] == DefKeys[0].Key1, "the first key must be the keyboard's Key1");
    CHECK(keys[1] == 0, "nothing may be written past max");
}

// The order is the fold's own: keyboard Key1, keyboard Key2, pad Key1, pad Key2.
// A caller pressing "the first one" is then pressing a keyboard key wherever the
// slot has one, which is what makes a re-expressed frame look like a person.
void CheckOrderIsKeyboardFirst() {
    Reset();
    U32 keys[4];
    S32 n = Bindings_KeysForSlot(0, keys, 4);
    CHECK(n >= 3, "retail slot 0 binds two keyboard keys and a pad button, got %d", (int)n);
    CHECK(keys[0] == DefKeys[0].Key1, "first is keyboard Key1");
    CHECK(keys[1] == DefKeys[0].Key2, "second is keyboard Key2");
    CHECK(keys[2] == GamepadKeys[0].Key1, "third is the pad");
}

// Unbound cells are skipped rather than handed back as zero, or a caller would
// press nothing and read it as a key.
void CheckUnboundCellsAreSkipped() {
    Reset();
    U32 keys[4];
    // I_THROW binds one keyboard key and one pad button, with both Key2 empty.
    S32 n = Bindings_KeysForSlot(4, keys, 4);
    for (S32 k = 0; k < n; k++)
        CHECK(keys[k] != 0, "slot 4 handed back an unbound cell at %d", (int)k);
}

void CheckRetailIsReversible() {
    Reset();
    S32 badSlot = -1;
    U32 badKey = 0;
    CHECK(Bindings_CheckReversible(&badSlot, &badKey) == BINDINGS_REVERSE_OK,
          "the retail layout must be reversible (slot %d, key %u)", (int)badSlot,
          (unsigned)badKey);
    CHECK(Bindings_CheckReversible(NULL, NULL) == BINDINGS_REVERSE_OK,
          "and the check must not need somewhere to report to");
}

// One key on two slots: pressing it for the held slot also raises the other, so
// a reverse that used it would produce a frame the recording never had.
void CheckSharedKeyIsRefused() {
    Reset();
    DefKeys[12].Key1 = DefKeys[0].Key1; // I_PAUSE onto I_UP's key
    InitInput();

    S32 badSlot = -1;
    U32 badKey = 0;
    CHECK(Bindings_CheckReversible(&badSlot, &badKey) == BINDINGS_REVERSE_SHARED,
          "a key on two slots must be refused");
    CHECK(badSlot == 12, "the later slot is the one that cannot be expressed, got %d",
          (int)badSlot);
    CHECK(badKey == DefKeys[0].Key1, "and the shared key must be named");

    // The reason it matters, stated through the fold rather than asserted.
    CHECK(InputFromHolding(DefKeys[0].Key1) == ((1u << 0) | (1u << 12)),
          "the shared key must raise both slots, which is why it is refused");
    Reset();
}

// A slot with nothing bound cannot be expressed at all.
void CheckUnboundSlotIsRefused() {
    Reset();
    DefKeys[13].Key1 = 0;
    DefKeys[13].Key2 = 0;
    GamepadKeys[13].Key1 = 0;
    GamepadKeys[13].Key2 = 0;
    InitInput();

    S32 badSlot = -1;
    U32 badKey = 1;
    CHECK(Bindings_CheckReversible(&badSlot, &badKey) == BINDINGS_REVERSE_UNBOUND,
          "a slot with no key must be refused");
    CHECK(badSlot == 13, "the empty slot must be named, got %d", (int)badSlot);
    CHECK(badKey == 0, "and there is no key to name");
    CHECK(Bindings_KeysForSlot(13, NULL, 0) == 0, "and it yields no keys");
    Reset();
}

// An empty slot is reported ahead of a sharing pair further down the table: it
// is the answer to "why did this action not come back", and burying it under a
// second fault further on would hide it.
void CheckUnboundIsReportedFirst() {
    Reset();
    DefKeys[20].Key1 = DefKeys[0].Key1; // a sharing fault, late in the table
    DefKeys[20].Key2 = 0;
    GamepadKeys[20].Key1 = 0;
    GamepadKeys[20].Key2 = 0;
    DefKeys[30].Key1 = 0; // and an empty slot, later still
    DefKeys[30].Key2 = 0;
    GamepadKeys[30].Key1 = 0;
    GamepadKeys[30].Key2 = 0;
    InitInput();

    S32 badSlot = -1;
    CHECK(Bindings_CheckReversible(&badSlot, NULL) == BINDINGS_REVERSE_UNBOUND,
          "the empty slot outranks the shared key");
    CHECK(badSlot == 30, "and it is the empty one that is named, got %d", (int)badSlot);
    Reset();
}

// A rebind is invisible to the fold until InitInput runs, and the reverse reads
// the source tables rather than the folded one, so it answers immediately. That
// difference is deliberate and worth pinning: a caller that rebinds and asks
// before refolding gets the new answer from one direction and the old from the
// other.
void CheckReverseReadsTheSourceTables() {
    Reset();
    DefKeys[12].Key1 = K_F11; // I_PAUSE moved, not yet folded

    CHECK(Bindings_SlotForKey(K_F11) == 12, "the reverse sees the rebind at once");
    CHECK(InputFromHolding(K_F11) == 0, "while the fold still does not");

    InitInput();
    CHECK(InputFromHolding(K_F11) == I_PAUSE, "and after the refold they agree");
    Reset();
}

} // namespace

int main() {
    CheckEveryRetailSlotIsReachable();
    CheckRoundTripThroughTheFold();
    CheckKeyNamesItsSlot();
    CheckUnboundMarkerIsNotAKey();
    CheckOutOfRangeSlots();
    CheckMaxIsRespected();
    CheckOrderIsKeyboardFirst();
    CheckUnboundCellsAreSkipped();
    CheckRetailIsReversible();
    CheckSharedKeyIsRefused();
    CheckUnboundSlotIsRefused();
    CheckUnboundIsReportedFirst();
    CheckReverseReadsTheSourceTables();

    if (fails == 0) {
        std::printf("test_input_reverse: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_reverse: %d failure(s)\n", fails);
    return 1;
}
