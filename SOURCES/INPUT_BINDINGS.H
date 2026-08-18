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
#define MAX_INPUT 36

typedef struct {
    U32 Key1;
    U32 Key2;
} T_DEF_KEY;

// The Windows 95 default layout: what RestoreInput installs, and the value a
// binding the config file leaves unset falls back to.
extern T_DEF_KEY DefKeysDefault95[MAX_INPUT + 1];

extern T_DEF_KEY DefKeys[MAX_INPUT + 1];
extern T_DEF_KEY MemoDefKeys[MAX_INPUT + 1];

// tableau de configuration des touches utilisé au cours du jeu
// Read by nothing in this build: the combined table InitInput hands to
// DefineInputKeys replaced it. Moved with the tables it belongs to rather than
// dropped, so the move stays a move.
extern U32 TabInputBit[MAX_INPUT * 2 + 1];

// Parallel gamepad binding table: Key1 / Key2 are alternate bindings (OR),
// same semantics as DefKeys (see KEYBOARD_KEYS.H for K_GAMEPAD_*). 0 =
// unbound. Folded into the combined input table by InitInput() alongside
// DefKeys, so the pad flows through GetInput()'s NoRepeatInput edge-filter.
extern T_DEF_KEY GamepadKeys[MAX_INPUT];
extern const T_DEF_KEY GamepadKeysDefault[MAX_INPUT];

// How many entries InitInput() handed to DefineInputKeys.
extern U32 NbInput;

//-------------------------------------------------------------------------
extern void InitInput(void);
//-------------------------------------------------------------------------
extern void RestoreInput(void);
//-------------------------------------------------------------------------

#endif // INPUT_BINDINGS_H
