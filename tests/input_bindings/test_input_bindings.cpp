/* Host test: the retail keyboard layout, and the fold that turns it into Input.
 *
 * Nothing linked SOURCES/INPUT.CPP before this test, so the table every player
 * plays on and the fold every action bit comes out of were both unchecked. They
 * are pure lookups over data -- no IO, no engine state -- which is why they can
 * be driven here at all.
 *
 * What this pins, in the terms docs/plan/INPUT_PLAN.md's parity rule sets out:
 *
 *   - DefKeysDefault95's action-to-key mapping, slot by slot. The port kept
 *     this one layout of the two the 1997 source carried (the DOS set is gone)
 *     and changed exactly one cell of it, so it is the retail keyboard as
 *     players know it and a change to it is a change to the game. Restated
 *     here rather than derived, so an edit to the table has to be an edit to
 *     this list too.
 *
 *   - The bits GetInput() produces for a given key state: every key of every
 *     slot below 32 raises that slot's bit and no other, and the four spell
 *     slots at 32-35 raise nothing.
 *
 * That second one is the whole point of the 32-slot ceiling. The spell bindings
 * live in a second bitfield -- INPUT.H gives I_PINGOUIN the value (1 << 0), the
 * same word as I_UP -- so folding them into the same table would make the
 * penguin key walk forward. PERSO.CPP reads them with a direct CheckKey on
 * DefKeys[32..35] instead. Nothing said so in a test until now.
 *
 * The probe is one key at a time: hold exactly one scancode, rebuild Input,
 * read it. That is unambiguous only because no two default bindings share a
 * key, which the test asserts before relying on it.
 */
#include <cstdio>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/INPUT.H>         // Input, GetInput, ClearNoRepeatInput
#include <SYSTEM/KEYBOARD_KEYS.H> // K_*

#include "INPUT.H" // SOURCES/INPUT.H: T_DEF_KEY, MAX_INPUT_SLOTS, InitInput, RestoreInput

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

// The retail layout, as players meet it. Slot order is the I_* bit order in
// SOURCES/INPUT.H: slots 0-31 are the Input word, slots 32-35 the four spell
// bits of the second word.
struct Binding {
    int slot;
    U32 key1;
    U32 key2;
    const char *action;
};

const Binding kRetail[] = {
    {0, K_GRAY_UP, K_NUMPAD_8, "I_UP"},
    {1, K_GRAY_DOWN, K_NUMPAD_2, "I_DOWN"},
    {2, K_GRAY_LEFT, K_NUMPAD_4, "I_LEFT"},
    {3, K_GRAY_RIGHT, K_NUMPAD_6, "I_RIGHT"},
    {4, K_ALT, 0, "I_THROW"},
    {5, K_CTRL, 0, "I_COMPORTEMENT"},
    {6, K_SHIFT, 0, "I_INVENTORY"},
    {7, K_SPACE, K_NUMPAD_5, "I_ACTION_M"},
    {8, K_W, K_GRAY_END, "I_ACTION_ALWAYS"},
    {9, K_ENTER, K_NUMPAD_ENTER, "I_RETURN"},
    {10, K_F10, 0, "I_MENUS"},
    {11, K_H, K_TAB, "I_HOLOMAP"},
    {12, K_P, 0, "I_PAUSE"},
    {13, K_X, 0, "I_ESQUIVE"},
    {14, K_F5, 0, "I_NORMAL"},
    {15, K_F6, 0, "I_SPORTIF"},
    {16, K_F7, 0, "I_AGRESSIF"},
    {17, K_F8, 0, "I_DISCRET"},
    {18, K_F1, 0, "I_HELP"},
    {19, K_F2, K_S, "I_SAVE"},
    {20, K_F3, K_L, "I_LOAD"},
    {21, K_F4, K_O, "I_OPTIONS"},
    // The one cell that differs from the 1997 table. Key2 was K_CARRE there,
    // the backtick INPUT.H still names in I_CAMERA's comment; the console
    // toggle owns that key now, so the alternate binding is gone and Backspace
    // is all that turns the camera. Every other cell below is the 1997 table
    // unchanged.
    {22, K_BACKSPACE, 0, "I_CAMERA"},
    {23, K_NUMPAD_PLUS, K_GRAY_PAGE_UP, "I_CAMERA_LEVEL_PLUS"},
    {24, K_NUMPAD_MOINS, K_GRAY_PAGE_DOWN, "I_CAMERA_LEVEL_MOINS"},
    {25, K_1, 0, "I_WEAPON_1"},
    {26, K_2, 0, "I_WEAPON_2"},
    {27, K_3, 0, "I_WEAPON_3"},
    {28, K_4, 0, "I_WEAPON_4"},
    {29, K_5, 0, "I_WEAPON_5"},
    {30, K_6, 0, "I_WEAPON_6"},
    {31, K_7, 0, "I_WEAPON_7"},
    // The second bitfield. Read by CheckKey on DefKeys, never folded.
    {32, K_N, 0, "I_PINGOUIN"},
    {33, K_J, 0, "I_JETPACK"},
    {34, K_C, 0, "I_PROTECTION"},
    {35, K_F, 0, "I_FOUDRE"},
};

const int kRetailCount = (int)(sizeof kRetail / sizeof kRetail[0]);

// Slots the fold reaches. Above this the bindings exist but stay out of Input.
const int kFoldedSlots = 32;

// --- 1. The layout, as data -------------------------------------------------
void CheckRetailLayout(void) {
    CHECK(kRetailCount == MAX_INPUT_SLOTS,
          "expected list covers %d slots, MAX_INPUT_SLOTS is %d", kRetailCount,
          (int)MAX_INPUT_SLOTS);

    RestoreInput();

    for (int i = 0; i < kRetailCount; i++) {
        const Binding &b = kRetail[i];
        // The probes below index this list by slot, so the two have to agree.
        CHECK(b.slot == i, "expected list is out of order at index %d", i);
        CHECK(DefKeys[b.slot].Key1 == b.key1,
              "%s (slot %d) Key1: got %u, expected %u", b.action, b.slot,
              DefKeys[b.slot].Key1, b.key1);
        CHECK(DefKeys[b.slot].Key2 == b.key2,
              "%s (slot %d) Key2: got %u, expected %u", b.action, b.slot,
              DefKeys[b.slot].Key2, b.key2);
    }

    // RestoreInput restores the pad in the same breath, so a restore-defaults
    // from the options screen cannot leave the two tables from different eras.
    for (int i = 0; i < MAX_INPUT_SLOTS; i++) {
        CHECK(GamepadKeys[i].Key1 == GamepadKeysDefault[i].Key1 &&
                  GamepadKeys[i].Key2 == GamepadKeysDefault[i].Key2,
              "slot %d: RestoreInput left the pad binding unrestored", i);
    }
}

// --- 2. No key serves two actions -------------------------------------------
// A player pressing one key means one thing. It is also what makes the probe
// below readable: one held scancode, one expected bit.
void CheckNoSharedKeys(void) {
    RestoreInput();

    for (int a = 0; a < MAX_INPUT_SLOTS; a++) {
        const U32 mine[2] = {DefKeys[a].Key1, DefKeys[a].Key2};
        for (int m = 0; m < 2; m++) {
            if (!mine[m])
                continue;
            CHECK(!(m == 1 && mine[0] == mine[1]),
                  "slot %d binds the same key twice (%u)", a, mine[0]);
            for (int b = a + 1; b < MAX_INPUT_SLOTS; b++) {
                CHECK(DefKeys[b].Key1 != mine[m] && DefKeys[b].Key2 != mine[m],
                      "key %u binds both slot %d (%s) and slot %d", mine[m], a,
                      kRetail[a].action, b);
            }
            // The pad shares the one table with the keyboard, so a collision
            // across the two would be the same bug.
            for (int b = 0; b < MAX_INPUT_SLOTS; b++) {
                CHECK(GamepadKeys[b].Key1 != mine[m] &&
                          GamepadKeys[b].Key2 != mine[m],
                      "key %u binds keyboard slot %d and pad slot %d", mine[m],
                      a, b);
            }
        }
    }
}

// --- 3. The fold, through GetInput ------------------------------------------
// Hold one key, rebuild, read Input. What comes out is the whole contract of
// InitInput's combined table, without inspecting the table itself.
U32 InputFromHolding(U32 key) {
    ClearNoRepeatInput();
    HoldOnly(key);
    GetInput(0);
    return Input;
}

void CheckFold(void) {
    RestoreInput();
    InitInput();

    // Four keys per slot -- keyboard Key1/Key2 and pad Key1/Key2 -- over the 32
    // slots the Input word has room for.
    CHECK(NbInput == (U32)(kFoldedSlots * 4),
          "combined table holds %u entries, expected %d", NbInput,
          kFoldedSlots * 4);

    ClearNoRepeatInput();
    HoldNone();
    GetInput(0);
    CHECK(Input == 0, "nothing held, Input=0x%08X", Input);

    for (int slot = 0; slot < kFoldedSlots; slot++) {
        const U32 expect = (U32)1 << slot;
        const U32 keys[4] = {DefKeys[slot].Key1, DefKeys[slot].Key2,
                             GamepadKeys[slot].Key1, GamepadKeys[slot].Key2};
        for (int k = 0; k < 4; k++) {
            if (!keys[k])
                continue; // unbound alternate
            const U32 got = InputFromHolding(keys[k]);
            CHECK(got == expect,
                  "%s (slot %d) key %u: Input=0x%08X, expected 0x%08X",
                  kRetail[slot].action, slot, keys[k], got, expect);
        }
    }
}

void CheckSpellSlotsStayOut(void) {
    RestoreInput();
    InitInput();

    // With the defaults: N, J, C and F reach Input as nothing at all.
    for (int slot = kFoldedSlots; slot < MAX_INPUT_SLOTS; slot++) {
        const U32 keys[2] = {DefKeys[slot].Key1, DefKeys[slot].Key2};
        for (int k = 0; k < 2; k++) {
            if (!keys[k])
                continue;
            const U32 got = InputFromHolding(keys[k]);
            CHECK(got == 0,
                  "%s (slot %d) key %u leaked into Input=0x%08X",
                  kRetail[slot].action, slot, keys[k], got);
        }
    }

    // And not because those particular keys are unbound elsewhere. Rebind the
    // four spell slots to scancodes nothing else uses and they still reach
    // nothing, which is the property: the ceiling is the slot index, not the
    // key. K_F11/K_F12 and the two unused numpad keys are bound by no default.
    const U32 sentinels[4] = {K_F11, K_F12, K_NUMPAD_7, K_NUMPAD_9};
    for (int i = 0; i < 4; i++) {
        DefKeys[kFoldedSlots + i].Key1 = sentinels[i];
        DefKeys[kFoldedSlots + i].Key2 = 0;
    }
    InitInput();
    for (int i = 0; i < 4; i++) {
        const U32 got = InputFromHolding(sentinels[i]);
        CHECK(got == 0, "rebound spell slot %d (key %u) leaked into Input=0x%08X",
              kFoldedSlots + i, sentinels[i], got);
    }
}

// --- 4. A rebind takes effect only once the table is rebuilt ----------------
// InitInput is what the remap screen, the config load and restore-defaults all
// have to call. A binding written into DefKeys and not folded is inert, and
// that is the failure every one of those paths risks.
void CheckRebindNeedsRefold(void) {
    RestoreInput();
    InitInput();

    CHECK(InputFromHolding(K_P) == I_PAUSE, "default: P raises I_PAUSE");

    DefKeys[12].Key1 = K_F11; // I_PAUSE moved
    CHECK(InputFromHolding(K_F11) == 0,
          "a rebind not yet folded must be inert");
    CHECK(InputFromHolding(K_P) == I_PAUSE,
          "and the old key must still answer until the refold");

    InitInput();
    CHECK(InputFromHolding(K_F11) == I_PAUSE, "after InitInput: F11 raises I_PAUSE");
    CHECK(InputFromHolding(K_P) == 0, "after InitInput: P is free");
}

} // namespace

int main() {
    CheckRetailLayout();
    CheckNoSharedKeys();
    CheckFold();
    CheckSpellSlotsStayOut();
    CheckRebindNeedsRefold();

    if (fails == 0) {
        std::printf("test_input_bindings: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_bindings: %d failure(s)\n", fails);
    return 1;
}
