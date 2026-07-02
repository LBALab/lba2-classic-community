/* Host test: the gamepad must be a first-class Input source.
 *
 * Fix under test: fold the gamepad bindings into GetInput()'s canonical Input
 * rebuild (one combined keyboard+gamepad key/mask table) instead of OR-ing the
 * gamepad bits into Input *after* GetInput() has already rebuilt and masked it
 * (the old ApplyGamepadBindings bolt-on in SOURCES/INPUT.CPP).
 *
 * GetInput() itself was always correct; the bug lived entirely in the wiring --
 * which table it walks, and whether the gamepad is OR-d in afterward. Two
 * regressions fell out of the old ordering:
 *
 *   Issue 1 (movement freeze) -- a mid-frame GetInput() such as
 *     InitWaitNoInput(I_RETURN) in the camera-recenter handler (PERSO.CPP)
 *     rebuilt Input from the keyboard table only and dropped the gamepad's
 *     movement bits for the rest of the frame. Holding the recenter button
 *     (gamepad B = I_RETURN) froze the character -> drowning in protopack /
 *     jetpack sections.
 *
 *   Issue 3 (overlays won't toggle closed) -- held gamepad menu/inventory/
 *     behaviour buttons were OR-d in after the NoRepeatInput edge-filter ran,
 *     so the same button that opened an overlay immediately re-opened it
 *     instead of toggling it closed.
 *
 * The test drives the real GetInput() against both wirings and asserts the new
 * combined table fixes the two scenarios the old wiring breaks. Lean link: only
 * LIB386/SYSTEM/INPUT.CPP -- CheckKey / ManageKeyboard are stubbed so the funnel
 * is driven purely from a held-scancode set, no SDL and no game stack.
 */
#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/KEYBOARD.H>      // CheckKey, ManageKeyboard (we define them)
#include <SYSTEM/KEYBOARD_KEYS.H> // K_GAMEPAD_*, K_ENTER, K_F10
#include <SYSTEM/INPUT.H>         // Input, GetInput, DefineInputKeys, ClearNoRepeatInput

#include "INPUT.H" // SOURCES/INPUT.H: I_* masks, T_DEF_KEY, MAX_INPUT, InitWaitNoInput

#include <cstdio>
#include <set>

// --- The two keyboard hooks GetInput() calls. Held keys are modelled as a set
// of scancodes; ManageKeyboard is a no-op (no SDL poll). This is what lets the
// test simulate a held button that persists across GetInput() calls -- exactly
// the mid-frame case that broke movement. ---
static std::set<U32> g_held;

extern "C" {
void ManageKeyboard(void) {}
S32 CheckKey(U32 key) { return g_held.count(key) ? 1 : 0; }
}

// C++98 helpers (the host tests build without C++11 brace-init for containers).
static void holdNone(void) { g_held.clear(); }
static void hold1(U32 a) {
    holdNone();
    g_held.insert(a);
}
static void hold2(U32 a, U32 b) {
    holdNone();
    g_held.insert(a);
    g_held.insert(b);
}

// --- Minimal binding tables for the inputs under test. Indexed by input slot,
// mirroring SOURCES/INPUT.CPP's DefKeys / GamepadKeys layout. ---
static T_DEF_KEY kbd[MAX_INPUT];
static T_DEF_KEY pad[MAX_INPUT];

static U32 combinedKeys[MAX_INPUT * 4];
static U32 combinedMasks[MAX_INPUT * 4];

static void setBinding(int slot, U32 kbd1, U32 kbd2, U32 pad1, U32 pad2) {
    kbd[slot].Key1 = kbd1;
    kbd[slot].Key2 = kbd2;
    pad[slot].Key1 = pad1;
    pad[slot].Key2 = pad2;
}

// NEW wiring: keyboard + gamepad in one table (what the fixed InitInput builds).
// GetInput() OR-s a slot's mask whenever ANY of its four keys is held, and the
// NoRepeatInput edge-filter then applies uniformly to keyboard and gamepad.
static void useCombinedTable(void) {
    S32 n = 0;
    for (int slot = 0; slot < 32; slot++) {
        U32 mask = (1u << slot);
        combinedKeys[n] = kbd[slot].Key1;
        combinedMasks[n++] = mask;
        combinedKeys[n] = kbd[slot].Key2;
        combinedMasks[n++] = mask;
        combinedKeys[n] = pad[slot].Key1;
        combinedMasks[n++] = mask;
        combinedKeys[n] = pad[slot].Key2;
        combinedMasks[n++] = mask;
    }
    DefineInputKeys(n, combinedKeys, combinedMasks);
}

// OLD wiring: keyboard-only table. The gamepad was applied separately, after the
// rebuild, by ApplyGamepadBindings (modelled by applyGamepadOR below).
static void useKeyboardOnlyTable(void) {
    S32 n = 0;
    for (int slot = 0; slot < 32; slot++) {
        U32 mask = (1u << slot);
        combinedKeys[n] = kbd[slot].Key1;
        combinedMasks[n++] = mask;
        combinedKeys[n] = kbd[slot].Key2;
        combinedMasks[n++] = mask;
    }
    DefineInputKeys(n, combinedKeys, combinedMasks);
}

// The old ApplyGamepadBindings: OR the held gamepad bits into Input *after*
// GetInput() has run -- bypassing the NoRepeatInput mask entirely.
static void applyGamepadOR(void) {
    for (int slot = 0; slot < 32; slot++) {
        if ((pad[slot].Key1 && CheckKey(pad[slot].Key1)) ||
            (pad[slot].Key2 && CheckKey(pad[slot].Key2)))
            Input |= (1u << slot);
    }
}

static int fails = 0;

static void expectSet(U32 bit, const char *label) {
    if (!(Input & bit)) {
        std::fprintf(stderr, "FAIL: %s: Input=0x%08X, expected bit 0x%08X set\n",
                     label, Input, bit);
        fails++;
    }
}

static void expectClear(U32 bit, const char *label) {
    if (Input & bit) {
        std::fprintf(stderr, "FAIL: %s: Input=0x%08X, expected bit 0x%08X clear\n",
                     label, Input, bit);
        fails++;
    }
}

int main() {
    // I_RIGHT: pad = right stick right. I_RETURN (recenter / "Select"): pad = B,
    // kbd = Enter. I_MENUS: pad = Start, kbd = F10.
    setBinding(3, 0, 0, K_GAMEPAD_RSTICK_RIGHT, 0); // I_RIGHT (bit 3)
    setBinding(9, K_ENTER, 0, K_GAMEPAD_B, 0);      // I_RETURN (bit 9)
    setBinding(10, K_F10, 0, K_GAMEPAD_START, 0);   // I_MENUS (bit 10)

    // Use the stick's digital right as the movement scancode the funnel emits.
    // (Slot 3 = I_RIGHT; the analog stick maps to K_GAMEPAD_RSTICK_RIGHT here
    // only to give the test a concrete held movement scancode.)

    /* =====================================================================
     * Issue 1 -- movement must survive a mid-frame GetInput (camera recenter).
     * Scenario: running right while holding the recenter button (B).
     * ===================================================================== */

    // NEW wiring: the combined table keeps the gamepad in every rebuild.
    useCombinedTable();
    ClearNoRepeatInput();
    hold2(K_GAMEPAD_RSTICK_RIGHT, K_GAMEPAD_B);
    GetInput(0);
    expectSet(I_RIGHT, "new: right held -> I_RIGHT");
    expectSet(I_RETURN, "new: B held -> I_RETURN");
    // Camera-recenter handler fires InitWaitNoInput(I_RETURN) mid-frame:
    InitWaitNoInput(I_RETURN); // == GetInput(I_RETURN)
    expectSet(I_RIGHT, "new: movement SURVIVES mid-frame GetInput (Issue 1 fixed)");

    // OLD wiring: the mid-frame rebuild is keyboard-only, so the gamepad
    // movement bit is dropped and never re-OR-d -> the frozen-character bug.
    useKeyboardOnlyTable();
    ClearNoRepeatInput();
    hold2(K_GAMEPAD_RSTICK_RIGHT, K_GAMEPAD_B);
    GetInput(0);
    applyGamepadOR();
    expectSet(I_RIGHT, "old: right held -> I_RIGHT (via post-OR)");
    InitWaitNoInput(I_RETURN); // mid-frame; the real path does NOT re-OR gamepad
    expectClear(I_RIGHT, "old BUG: movement dropped by mid-frame GetInput");

    /* =====================================================================
     * Issue 3 -- held menu button must edge-detect (overlay toggles closed).
     * Scenario: holding Start (I_MENUS); the overlay arms NoRepeatInput on it.
     * ===================================================================== */

    // NEW wiring: the arm bites on the gamepad, so a held Start fires once.
    useCombinedTable();
    ClearNoRepeatInput();
    hold1(K_GAMEPAD_START);
    GetInput(0);
    expectSet(I_MENUS, "new: Start press -> I_MENUS (frame 1)");
    InitWaitNoInput(I_MENUS); // overlay arms the edge-filter on open
    GetInput(0);              // still held
    expectClear(I_MENUS, "new: held Start masked -> overlay stays closed (Issue 3 fixed)");
    holdNone();
    GetInput(0); // release seen -> NoRepeatInput drops the bit
    hold1(K_GAMEPAD_START);
    GetInput(0);
    expectSet(I_MENUS, "new: re-press after release fires again");

    // OLD wiring: the post-OR re-adds Start every frame, bypassing the arm ->
    // the overlay re-opens instead of closing.
    useKeyboardOnlyTable();
    ClearNoRepeatInput();
    hold1(K_GAMEPAD_START);
    GetInput(0);
    applyGamepadOR();
    expectSet(I_MENUS, "old: Start -> I_MENUS (via post-OR)");
    InitWaitNoInput(I_MENUS); // arm has nothing to bite on (gamepad not in table)
    GetInput(0);
    applyGamepadOR();
    expectSet(I_MENUS, "old BUG: held Start keeps firing I_MENUS (bypasses edge-filter)");

    if (fails == 0) {
        std::printf("test_input_funnel: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_funnel: %d failure(s)\n", fails);
    return 1;
}
