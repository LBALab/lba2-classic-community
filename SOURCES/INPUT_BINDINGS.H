#ifndef INPUT_BINDINGS_H
#define INPUT_BINDINGS_H

/* Which physical key carries which intent, and the fold that turns that into
 * the Input bitfield.
 *
 * Pure lookups over data: no file IO, no polling, no engine state. That is the
 * line this module is cut along, and what lets tests/input_bindings drive it
 * with nothing underneath but DefineInputKeys. INPUT.CPP keeps the halves that
 * cannot be: reading and writing the config file, and sampling the keyboard,
 * the pad and the console every frame.
 *
 * INPUT.H includes this header, so the eighteen files that already include it
 * see the tables as before. Narrowing that to the handful that actually rebind
 * keys belongs with the work that gives the config screen and the cfg reader
 * one entry point each, not with the move.
 */

#include <SYSTEM/ADELINE_TYPES.H>

// The number of bindable actions. The first 32 are the Input word (INPUT.H's
// I_* bits, one per slot); the last four are the spell bits of a second word,
// which is why they cannot share the folded table -- I_PINGOUIN is (1 << 0),
// the same bit as I_UP. PERSO.CPP reads those four with a direct CheckKey.
//
// Not spelled MAX_INPUT: that name belongs to POSIX. <linux/limits.h> gives it
// the value 255, the size of the terminal type-ahead buffer, and Bionic reaches
// it through <limits.h> where desktop glibc does not. A translation unit that
// includes this header ahead of a system header would then declare these tables
// at one size and define them at another, which the compiler reports as a
// redefinition with a different type. A name this module owns cannot be
// clobbered by include order, and that, rather than the number, is the point.
#define MAX_INPUT_SLOTS 36

typedef struct {
    U32 Key1;
    U32 Key2;
} T_DEF_KEY;

// The Windows 95 default layout: what RestoreInput installs, and the value a
// binding the config file leaves unset falls back to. Const like the pad table
// below it, since a default nothing may edit is the point of having one.
extern const T_DEF_KEY DefKeysDefault95[MAX_INPUT_SLOTS + 1];

extern T_DEF_KEY DefKeys[MAX_INPUT_SLOTS + 1];
extern T_DEF_KEY MemoDefKeys[MAX_INPUT_SLOTS + 1];

// tableau de configuration des touches utilisé au cours du jeu
// Read by nothing in this build: the combined table InitInput hands to
// DefineInputKeys replaced it. Moved with the tables it belongs to rather than
// dropped, so the move stays a move.
extern U32 TabInputBit[MAX_INPUT_SLOTS * 2 + 1];

// Parallel gamepad binding table: Key1 / Key2 are alternate bindings (OR),
// same semantics as DefKeys (see KEYBOARD_KEYS.H for K_GAMEPAD_*). 0 =
// unbound. Folded into the combined input table by InitInput() alongside
// DefKeys, so the pad flows through GetInput()'s NoRepeatInput edge-filter.
extern T_DEF_KEY GamepadKeys[MAX_INPUT_SLOTS];
extern const T_DEF_KEY GamepadKeysDefault[MAX_INPUT_SLOTS];

// How many entries InitInput() handed to DefineInputKeys.
extern U32 NbInput;

//-------------------------------------------------------------------------
extern void InitInput(void);
//-------------------------------------------------------------------------
extern void RestoreInput(void);
//-------------------------------------------------------------------------

// The other direction: which keys carry a slot, and which slot a key carries.
//
// InitInput folds the two tables into the flat (key, mask) array GetInput scans,
// which answers "is this slot held" and nothing else. Three readers want the
// question the other way round and each does it by hand today: the key-config
// screen indexes DefKeys positionally to say what a slot is bound to, a profile
// system would need the same to name a binding, and anything re-expressing an
// action as a key press needs a key to press.
//
// Slot indices run 0..MAX_INPUT_SLOTS-1 and cover the spells at 32-35, which the
// combined table stops short of: they have no Input bit (see INPUT_TABLE_SLOTS
// in the .CPP) but they are bound the same way and PERSO.CPP polls them with the
// same OR of four keys. The reverse direction has no reason to inherit that
// ceiling, so it does not.

// Keys bound to `slot`, written to `out` in the order the forward fold reads
// them: keyboard Key1, keyboard Key2, pad Key1, pad Key2, skipping unbound
// zeroes. Returns how many were written, so 0 means the slot cannot be
// expressed as a key press at all.
extern S32 Bindings_KeysForSlot(S32 slot, U32 *out, S32 max);

// The slot `key` is bound to, or -1 for a key no slot claims. The first match in
// slot order when a table binds one key twice, which Bindings_CheckReversible
// is what refuses.
extern S32 Bindings_SlotForKey(U32 key);

// Whether the live tables can be read backwards without changing meaning, which
// is not a property the forward direction needs and not one a cfg guarantees.
// Two ways it fails, and each is a different repair, so they are reported apart:
//
//   BINDINGS_REVERSE_SHARED   one key on two slots, so pressing it for the slot
//                             that is held also raises the slot that is not.
//   BINDINGS_REVERSE_UNBOUND  a slot with all four keys zero, so a held slot has
//                             no key to press.
//
// `badSlot` and `badKey` receive the first offender, either may be NULL.
#define BINDINGS_REVERSE_OK 0
#define BINDINGS_REVERSE_SHARED 1
#define BINDINGS_REVERSE_UNBOUND 2
extern S32 Bindings_CheckReversible(S32 *badSlot, U32 *badKey);

#endif // INPUT_BINDINGS_H
