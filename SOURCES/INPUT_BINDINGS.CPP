#include "INPUT_BINDINGS.H"

#include <string.h> // memcpy

#include <SYSTEM/INPUT.H>         // DefineInputKeys
#include <SYSTEM/KEYBOARD_KEYS.H> // K_*

// The 1997 source carried two default layouts, DefKeysDefault for DOS and this
// one for Windows 95. Only the second was ported. A binding set is where the
// first would come back if anyone wants it.
const T_DEF_KEY DefKeysDefault95[MAX_INPUT + 1] =
    {
        {K_GRAY_UP, K_NUMPAD_8},    // I_UP
        {K_GRAY_DOWN, K_NUMPAD_2},  // I_DOWN
        {K_GRAY_LEFT, K_NUMPAD_4},  // I_LEFT
        {K_GRAY_RIGHT, K_NUMPAD_6}, // I_RIGHT
        {K_ALT, 0},                 // I_THROW
        {K_CTRL, 0},                // I_COMPORTEMENT
        {K_SHIFT, 0},               // I_INVENTORY
        {K_SPACE, K_NUMPAD_5},      // I_ACTION_M
        {K_W, K_GRAY_END},          // I_ACTION_ALWAYS
        {K_ENTER, K_NUMPAD_ENTER},  // I_RETURN
        {K_F10, 0},                 // I_MENUS
        {K_H, K_TAB},               // I_HOLOMAP
        {K_P, 0},                   // I_PAUSE
        {K_X, 0},                   // I_ESQUIVE
        {K_F5, 0},                  // I_NORMAL
        {K_F6, 0},                  // I_SPORTIF
        {K_F7, 0},                  // I_AGRESSIF
        {K_F8, 0},                  // I_DISCRET
        {K_F1, 0},                  // I_HELP
        {K_F2, K_S},                // I_SAVE
        {K_F3, K_L},                // I_LOAD
        {K_F4, K_O},                // I_OPTIONS
        {K_BACKSPACE,
         0},                                // I_CAMERA (backtick used for console)
        {K_NUMPAD_PLUS, K_GRAY_PAGE_UP},    // I_CAMERA_LEVEL_PLUS
        {K_NUMPAD_MOINS, K_GRAY_PAGE_DOWN}, // I_CAMERA_LEVEL_MOINS
        {K_1, 0},                           // I_WEAPON_1
        {K_2, 0},                           // I_WEAPON_2
        {K_3, 0},                           // I_WEAPON_3
        {K_4, 0},                           // I_WEAPON_4
        {K_5, 0},                           // I_WEAPON_5
        {K_6, 0},                           // I_WEAPON_6
        {K_7, 0},                           // I_WEAPON_7
        {K_N, 0},                           // I_PINGOUIN
        {K_J, 0},                           // I_JETPACK
        {K_C, 0},                           // I_PROTECTION
        {K_F, 0},                           // I_FOUDRE
};

T_DEF_KEY MemoDefKeys[MAX_INPUT + 1];
T_DEF_KEY DefKeys[MAX_INPUT + 1];

// tableau de configuration des touches utilisé au cours du jeux
U32 TabInputBit[MAX_INPUT * 2 + 1];

// Combined keyboard+gamepad key/mask table handed to GetInput(). Folding the
// gamepad in here (instead of OR-ing it into Input afterward) makes it a
// first-class input: it flows through the one Input rebuild + NoRepeatInput
// edge-filter, so held pad buttons edge-detect like keyboard keys and survive a
// mid-frame GetInput() rebuild. Only the first 32 slots map to Input bits; four
// keys per slot (keyboard Key1/Key2 + gamepad Key1/Key2) => 32 * 4 entries.
#define INPUT_TABLE_SLOTS 32
static U32 CombinedInputKeys[INPUT_TABLE_SLOTS * 4];
static U32 CombinedInputMasks[INPUT_TABLE_SLOTS * 4];

T_DEF_KEY GamepadKeys[MAX_INPUT];

const T_DEF_KEY GamepadKeysDefault[MAX_INPUT] = {
    {K_GAMEPAD_RTRIGGER, 0},                         // 0  I_UP
    {K_GAMEPAD_LTRIGGER, 0},                         // 1  I_DOWN
    {K_GAMEPAD_LSTICK_LEFT, 0},                      // 2  I_LEFT
    {K_GAMEPAD_LSTICK_RIGHT, 0},                     // 3  I_RIGHT
    {K_GAMEPAD_RSHOULDER, 0},                        // 4  I_THROW
    {K_GAMEPAD_LSHOULDER, 0},                        // 5  I_COMPORTEMENT
    {K_GAMEPAD_Y, 0},                                // 6  I_INVENTORY
    {K_GAMEPAD_X, 0},                                // 7  I_ACTION_M
    {K_GAMEPAD_A, 0},                                // 8  I_ACTION_ALWAYS
    {K_GAMEPAD_B, 0},                                // 9  I_RETURN
    {K_GAMEPAD_START, 0},                            // 10 I_MENUS
    {K_GAMEPAD_BACK, 0},                             // 11 I_HOLOMAP
    {0, 0},                                          // 12 I_PAUSE (unbound; Start opens the in-game menu)
    {K_GAMEPAD_LSTICK_BTN, K_GAMEPAD_RSTICK_BTN},    // 13 I_ESQUIVE
    {0, 0},                                          // 14 I_NORMAL
    {0, 0},                                          // 15 I_SPORTIF
    {0, 0},                                          // 16 I_AGRESSIF
    {0, 0},                                          // 17 I_DISCRET
    {0, 0},                                          // 18 I_HELP
    {0, 0},                                          // 19 I_SAVE
    {0, 0},                                          // 20 I_LOAD
    {0, 0},                                          // 21 I_OPTIONS
    {K_GAMEPAD_RSTICK_LEFT, K_GAMEPAD_RSTICK_RIGHT}, // 22 I_CAMERA (classic-mode turn; analog orbit owns Auto camera)
    {K_GAMEPAD_RSTICK_UP, 0},                        // 23 I_CAMERA_LEVEL_PLUS
    {K_GAMEPAD_RSTICK_DOWN, 0},                      // 24 I_CAMERA_LEVEL_MOINS
    {0, 0},                                          // 25 I_WEAPON_1
    {0, 0},                                          // 26
    {0, 0},                                          // 27
    {0, 0},                                          // 28
    {0, 0},                                          // 29
    {0, 0},                                          // 30
    {0, 0},                                          // 31
    {K_GAMEPAD_DPAD_RIGHT, 0},                       // 32 I_PINGOUIN
    {K_GAMEPAD_DPAD_DOWN, 0},                        // 33 I_JETPACK
    {K_GAMEPAD_DPAD_UP, 0},                          // 34 I_PROTECTION
    {K_GAMEPAD_DPAD_LEFT, 0},                        // 35 I_FOUDRE
};

U32 NbInput = MAX_INPUT;

//-------------------------------------------------------------------------
void InitInput(void) {
    // Rebuild the combined table from the current keyboard (DefKeys) and gamepad
    // (GamepadKeys) bindings. Call after either changes (key remap, config load,
    // restore-defaults). Spell slots 32-35 stay out of the Input path and are
    // read via direct CheckKey(DefKeys[..]) in PERSO.CPP.
    S32 n = 0;
    for (S32 i = 0; i < INPUT_TABLE_SLOTS; i++) {
        U32 mask = (1u << i);
        CombinedInputKeys[n] = DefKeys[i].Key1;
        CombinedInputMasks[n++] = mask;
        CombinedInputKeys[n] = DefKeys[i].Key2;
        CombinedInputMasks[n++] = mask;
        CombinedInputKeys[n] = GamepadKeys[i].Key1;
        CombinedInputMasks[n++] = mask;
        CombinedInputKeys[n] = GamepadKeys[i].Key2;
        CombinedInputMasks[n++] = mask;
    }

    NbInput = (U32)n;

    DefineInputKeys(n, CombinedInputKeys, CombinedInputMasks);
}

//-------------------------------------------------------------------------
void RestoreInput() {
    const T_DEF_KEY *DEFKEYSDEFAULT;
    DEFKEYSDEFAULT = DefKeysDefault95;

    memcpy(DefKeys, DEFKEYSDEFAULT, sizeof(T_DEF_KEY) * MAX_INPUT);
    memcpy(GamepadKeys, GamepadKeysDefault, sizeof(GamepadKeys));
}
