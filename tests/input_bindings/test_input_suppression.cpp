/* Host test: NoRepeatInput, the latch that turns a held key into one press.
 *
 * Every screen that opens on a key and closes on the same key depends on this,
 * and it is twenty-six lines in LIB386/SYSTEM/INPUT.CPP:
 *
 *     NoRepeatInput |= norepeat;         // arm
 *     Input = 0; ...rebuild from the table...
 *     norepeat = NoRepeatInput & Input;  // keep only the bits still held
 *     Input &= ~NoRepeatInput;           // mask, with the value from before
 *     NoRepeatInput = norepeat;
 *
 * tests/input_funnel drives it already, but as the vehicle for two gamepad
 * wiring scenarios rather than as the subject, and on a synthetic three-slot
 * table. The rules themselves were never written down anywhere, which is a
 * problem for the increment that gives suppression an owner: moving it needs a
 * statement of what it does now to move it against.
 *
 * Five rules, in the order the code produces them:
 *
 *   1. Arming a bit that is not held does nothing, and does not survive the
 *      call it was asked in. The bit goes in, the narrowing drops it because it
 *      is not in Input, and the store writes the narrowed value back. So
 *      InitWaitNoInput(X) arms only if X is down at that moment.
 *
 *   2. Arming a bit that IS held masks it in that same call, so the screen that
 *      just opened does not read the key that opened it as a second press.
 *
 *      The mask and the store are interchangeable, which is not obvious from
 *      reading them. Masking with the armed set and masking with the narrowed
 *      set clear the same bits, because the mask can only reach a bit that is
 *      both armed and held and that is exactly what the narrowing keeps.
 *      Recorded because the order looks load-bearing and is not: swapping the
 *      two lines leaves every check below passing.
 *
 *   3. One frame with the bit absent is the only automatic way out, and it is
 *      per bit: releasing one armed key leaves every other latch alone.
 *
 *   4. Arming accumulates. Two screens can each arm their own bit and neither
 *      disturbs the other's.
 *
 *   5. ClearNoRepeatInput clears ALL of it. There is no per-bit release, so a
 *      screen dropping its own arm drops everyone else's with it. That is the
 *      shape of the problem, recorded here so a fix has a before.
 *
 * A sixth thing falls out of where the latch sits: it is on the action bit,
 * after every device has been folded in, so it cannot tell which device raised
 * the bit and switching device mid-hold does not re-trigger.
 */
#include <cstdio>

#include <SYSTEM/ADELINE_TYPES.H>
#include <SYSTEM/INPUT.H>         // Input, GetInput, ClearNoRepeatInput
#include <SYSTEM/KEYBOARD_KEYS.H> // K_*

#include "INPUT.H" // SOURCES/INPUT.H: I_* masks, InitWaitNoInput

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

// The real retail bindings, so the keys below are the keys a player presses.
// F10 is I_MENUS, H is I_HOLOMAP, the up arrow is I_UP.
void UseRetailBindings(void) {
    RestoreInput();
    InitInput();
    ClearNoRepeatInput();
    HoldNone();
}

void ExpectSet(U32 bit, const char *what) {
    CHECK((Input & bit) != 0, "%s: Input=0x%08X, expected 0x%08X set", what, Input, bit);
}

void ExpectClear(U32 bit, const char *what) {
    CHECK((Input & bit) == 0, "%s: Input=0x%08X, expected 0x%08X clear", what, Input, bit);
}

// --- 1. Arming a bit nothing is holding ------------------------------------
void CheckArmingAnUnheldBitDoesNothing(void) {
    UseRetailBindings();

    GetInput(I_MENUS); // arm with F10 up
    ExpectClear(I_MENUS, "nothing held");

    HoldOnly(K_F10);
    GetInput(0);
    ExpectSet(I_MENUS, "an arm taken while the key was up must not outlive its call");
}

// --- 2. Arming a bit that is held ------------------------------------------
void CheckArmingAHeldBitMasksItAtOnce(void) {
    UseRetailBindings();

    HoldOnly(K_F10);
    GetInput(0);
    ExpectSet(I_MENUS, "press");

    // What a screen does on open: InitWaitNoInput on the key that opened it.
    InitWaitNoInput(I_MENUS); // == GetInput(I_MENUS)
    ExpectClear(I_MENUS, "the arming call must itself come back masked");

    GetInput(0);
    ExpectClear(I_MENUS, "and stay masked while the key is down");

    // Other actions are untouched by another bit's latch.
    HoldAdd(K_GRAY_UP);
    GetInput(0);
    ExpectClear(I_MENUS, "still masked");
    ExpectSet(I_UP, "a latch on one bit must not reach another");
}

// --- 3. Release is the only way out, and it is per bit ----------------------
void CheckReleaseClearsOnlyItsOwnBit(void) {
    UseRetailBindings();

    HoldOnly(K_F10);
    HoldAdd(K_H);
    GetInput(0);
    ExpectSet(I_MENUS, "F10 press");
    ExpectSet(I_HOLOMAP, "H press");

    GetInput(I_MENUS);   // one screen arms its own bit
    GetInput(I_HOLOMAP); // and another arms its own, a call later
    ExpectClear(I_MENUS, "F10 masked");
    ExpectClear(I_HOLOMAP, "H masked");

    GetInput(0);
    ExpectClear(I_MENUS, "F10 still masked (arming accumulates)");
    ExpectClear(I_HOLOMAP, "H still masked");

    HoldRemove(K_F10); // release one of the two
    GetInput(0);
    ExpectClear(I_MENUS, "released, so nothing to report");
    ExpectClear(I_HOLOMAP, "the other key is still down and still masked");

    HoldAdd(K_F10); // press it again
    GetInput(0);
    ExpectSet(I_MENUS, "one frame of release is enough to re-arm the press");
    ExpectClear(I_HOLOMAP, "and it must not have cleared the other latch");
}

// --- 4. There is no per-bit clear -------------------------------------------
// The rule increment 2 of docs/plan/INPUT_PLAN.md exists to change. Recorded as
// it stands so the change has a before to be measured against.
void CheckClearIsAllOrNothing(void) {
    UseRetailBindings();

    HoldOnly(K_F10);
    HoldAdd(K_H);
    GetInput(0);
    GetInput(I_MENUS);
    GetInput(I_HOLOMAP);
    ExpectClear(I_MENUS, "both armed");
    ExpectClear(I_HOLOMAP, "both armed");

    ClearWaitNoInput(); // == ClearNoRepeatInput()
    GetInput(0);
    ExpectSet(I_MENUS, "one screen's clear releases its own latch");
    ExpectSet(I_HOLOMAP, "and every other screen's along with it");
}

// --- 5. The latch is on the action, not on the device -----------------------
// It sits after the fold, so it cannot see which of a slot's four keys raised
// the bit. Swapping device without letting the bit fall is therefore not a new
// press, which is the behaviour a per-device latch would not have.
void CheckLatchIsSourceBlind(void) {
    UseRetailBindings();

    HoldOnly(K_F10); // keyboard
    GetInput(0);
    ExpectSet(I_MENUS, "keyboard press");
    GetInput(I_MENUS);
    ExpectClear(I_MENUS, "masked");

    HoldAdd(K_GAMEPAD_START); // pad down before the key comes up
    HoldRemove(K_F10);
    GetInput(0);
    ExpectClear(I_MENUS, "the bit never fell, so changing device is not a new press");

    HoldNone();
    GetInput(0);
    HoldOnly(K_GAMEPAD_START);
    GetInput(0);
    ExpectSet(I_MENUS, "and the pad fires once the bit has actually been released");
}

} // namespace

int main() {
    CheckArmingAnUnheldBitDoesNothing();
    CheckArmingAHeldBitMasksItAtOnce();
    CheckReleaseClearsOnlyItsOwnBit();
    CheckClearIsAllOrNothing();
    CheckLatchIsSourceBlind();

    if (fails == 0) {
        std::printf("test_input_suppression: all checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "test_input_suppression: %d failure(s)\n", fails);
    return 1;
}
