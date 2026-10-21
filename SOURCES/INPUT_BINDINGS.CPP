#include "INPUT_BINDINGS.H"

#include <string.h> // memcpy

#include <SYSTEM/INPUT.H>         // DefineInputKeys
#include <SYSTEM/KEYBOARD.H>      // CheckKey
#include <SYSTEM/KEYBOARD_KEYS.H> // K_*
#include <INPUT_PROFILE_CONFIG.H>

// The symbol retains the Windows 95 name used by the config reader; the
// build-time keyboard profile supplies the defaults.
const T_DEF_KEY DefKeysDefault95[MAX_INPUT_SLOTS + 1] =
    {LBA2_INPUT_KEYBOARD_DEFAULTS};

T_DEF_KEY MemoDefKeys[MAX_INPUT_SLOTS + 1];
T_DEF_KEY DefKeys[MAX_INPUT_SLOTS + 1];

// tableau de configuration des touches utilisé au cours du jeux
U32 TabInputBit[MAX_INPUT_SLOTS * 2 + 1];

// Combined keyboard+gamepad key/mask table handed to GetInput(). Folding the
// gamepad in here (instead of OR-ing it into Input afterward) makes it a
// first-class input: it flows through the one Input rebuild + NoRepeatInput
// edge-filter, so held pad buttons edge-detect like keyboard keys and survive a
// mid-frame GetInput() rebuild. Only the first 32 slots map to Input bits; four
// keys per slot (keyboard Key1/Key2 + gamepad Key1/Key2) => 32 * 4 entries.
#define INPUT_TABLE_SLOTS 32
static U32 CombinedInputKeys[INPUT_TABLE_SLOTS * 4];
static U32 CombinedInputMasks[INPUT_TABLE_SLOTS * 4];

T_DEF_KEY GamepadKeys[MAX_INPUT_SLOTS];

const T_DEF_KEY GamepadKeysDefault[MAX_INPUT_SLOTS] = {LBA2_INPUT_GAMEPAD_DEFAULTS};

U32 NbInput = MAX_INPUT_SLOTS;

typedef struct {
    U32 key;
    S32 gameplaySlot;
    S32 modalSlot;
} T_CONTEXT_REMAP;

static const T_CONTEXT_REMAP ContextRemaps[] = {LBA2_INPUT_MODAL_REMAPS{0, 0, 0}};
enum { NUM_CONTEXT_REMAPS = sizeof(ContextRemaps) / sizeof(ContextRemaps[0]) - 1 };
static S32 s_gameplay = 0;
static U8 s_contextSuppressed[NUM_CONTEXT_REMAPS + 1];

static U32 ContextMask(S32 slot, U32 key, S32 gameplay) {
    if (!gameplay) {
        for (S32 n = 0; n < NUM_CONTEXT_REMAPS; ++n) {
            const T_CONTEXT_REMAP *remap = &ContextRemaps[n];
            if (remap->key == key && remap->gameplaySlot == slot)
                return 1u << remap->modalSlot;
        }
    }
    return 1u << slot;
}

static U32 KeyContextMask(U32 key, S32 gameplay) {
    U32 mask = 0;
    for (S32 slot = 0; slot < INPUT_TABLE_SLOTS; ++slot) {
        U32 keys[4];
        S32 count = Bindings_KeysForSlot(slot, keys, 4);
        for (S32 n = 0; n < count; ++n)
            if (keys[n] == key)
                mask |= ContextMask(slot, key, gameplay);
    }
    return mask;
}

static S32 KeyContextSuppressed(U32 key) {
    for (S32 n = 0; n < NUM_CONTEXT_REMAPS; ++n)
        if (ContextRemaps[n].key == key && s_contextSuppressed[n])
            return 1;
    return 0;
}

//-------------------------------------------------------------------------
void InitInput(void) {
    // Rebuild the combined table from the current keyboard (DefKeys) and gamepad
    // (GamepadKeys) bindings. Call after either changes (key remap, config load,
    // restore-defaults). Spell slots 32-35 stay out of the Input path and are
    // read via direct CheckKey(DefKeys[..]) in PERSO.CPP.
    S32 n = 0;
    for (S32 i = 0; i < INPUT_TABLE_SLOTS; i++) {
        const U32 keys[4] = {DefKeys[i].Key1, DefKeys[i].Key2,
                             GamepadKeys[i].Key1, GamepadKeys[i].Key2};
        for (S32 k = 0; k < 4; ++k) {
            CombinedInputKeys[n] = keys[k];
            CombinedInputMasks[n++] = KeyContextSuppressed(keys[k]) ? 0 : ContextMask(i, keys[k], s_gameplay);
        }
    }

    NbInput = (U32)n;

    DefineInputKeys(n, CombinedInputKeys, CombinedInputMasks);
}

void Bindings_SetGameplay(S32 gameplay) {
    gameplay = gameplay != 0;
    S32 rebuild = s_gameplay != gameplay && NUM_CONTEXT_REMAPS != 0;
    for (S32 n = 0; n < NUM_CONTEXT_REMAPS; ++n) {
        U32 key = ContextRemaps[n].key;
        U8 suppressed = s_contextSuppressed[n];
        if (!CheckKey(key))
            suppressed = 0;
        else if (s_gameplay != gameplay &&
                 KeyContextMask(key, s_gameplay) != KeyContextMask(key, gameplay))
            suppressed = 1;
        if (s_contextSuppressed[n] != suppressed)
            rebuild = 1;
        s_contextSuppressed[n] = suppressed;
    }
    s_gameplay = gameplay;
    if (rebuild)
        InitInput();
}

//-------------------------------------------------------------------------
// The reverse of the fold above. Pure lookups over the two tables, in the same
// order InitInput reads them, so a key this hands back is one the forward
// direction would have folded into that slot's mask.
S32 Bindings_KeysForSlot(S32 slot, U32 *out, S32 max) {
    S32 n = 0;

    if (slot < 0 OR slot >= MAX_INPUT_SLOTS OR out == NULL)
        return 0;

    const U32 candidates[4] = {DefKeys[slot].Key1, DefKeys[slot].Key2,
                               GamepadKeys[slot].Key1, GamepadKeys[slot].Key2};
    for (S32 i = 0; i < 4 AND n < max; i++) {
        if (candidates[i] != 0)
            out[n++] = candidates[i];
    }
    return n;
}

//-------------------------------------------------------------------------
S32 Bindings_SlotForKey(U32 key) {
    // Zero is the unbound marker in both tables, not a key. Answering the
    // question for it would report every empty cell as a match.
    if (key == 0)
        return -1;

    // Through the same helper rather than a second copy of the four-cell read,
    // so the two directions cannot drift apart on which cells count.
    for (S32 i = 0; i < MAX_INPUT_SLOTS; i++) {
        U32 keys[4];
        S32 n = Bindings_KeysForSlot(i, keys, 4);
        for (S32 k = 0; k < n; k++) {
            if (keys[k] == key)
                return i;
        }
    }
    return -1;
}

//-------------------------------------------------------------------------
S32 Bindings_CheckReversible(S32 *badSlot, U32 *badKey) {
    // Unbound first: a slot with no key is the answer to "why did this action
    // not come back", and reporting a sharing pair from further down the table
    // would bury it.
    for (S32 i = 0; i < MAX_INPUT_SLOTS; i++) {
        U32 keys[4];
        if (Bindings_KeysForSlot(i, keys, 4) == 0) {
            if (badSlot)
                *badSlot = i;
            if (badKey)
                *badKey = 0;
            return BINDINGS_REVERSE_UNBOUND;
        }
    }

    for (S32 i = 0; i < MAX_INPUT_SLOTS; i++) {
        U32 keys[4];
        S32 n = Bindings_KeysForSlot(i, keys, 4);
        for (S32 k = 0; k < n; k++) {
            // The lookup scans in slot order, so a key names the lowest slot
            // that binds it. A key of this slot naming a different one is the
            // sharing case, and it is this slot that cannot be expressed.
            S32 owner = Bindings_SlotForKey(keys[k]);
            if (owner != i) {
                if (badSlot)
                    *badSlot = i;
                if (badKey)
                    *badKey = keys[k];
                return BINDINGS_REVERSE_SHARED;
            }
        }
    }

    return BINDINGS_REVERSE_OK;
}

//-------------------------------------------------------------------------
void RestoreInput() {
    const T_DEF_KEY *DEFKEYSDEFAULT;
    DEFKEYSDEFAULT = DefKeysDefault95;

    memcpy(DefKeys, DEFKEYSDEFAULT, sizeof(T_DEF_KEY) * MAX_INPUT_SLOTS);
    memcpy(GamepadKeys, GamepadKeysDefault, sizeof(GamepadKeys));
}
