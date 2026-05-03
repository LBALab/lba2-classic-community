#include "C_EXTERN.H"

#include <stdio.h>

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include "INPUT.H"
#include "JOYSTICK.H"

#include "CONSOLE/CONSOLE.H"
#include <SYSTEM/INPUT.H>
#include <SVGA/SCREEN.H>
#include <SVGA/DIRTYBOX.H>

// Input
U32 WaitNoKey = 0;

#ifdef LBA_EDITOR
U32 WaitNoClick = 0;
#endif

U32 LastInput = 0;
U32 NbInput = MAX_INPUT;

T_DEF_KEY DefKeysDefault95[MAX_INPUT + 1] =
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

T_DEF_KEY GamepadKeys[MAX_INPUT];

const T_DEF_KEY GamepadKeysDefault[MAX_INPUT] = {
    {K_GAMEPAD_RTRIGGER, 0},                      // 0  I_UP
    {K_GAMEPAD_LTRIGGER, 0},                      // 1  I_DOWN
    {K_GAMEPAD_LSTICK_LEFT, 0},                   // 2  I_LEFT
    {K_GAMEPAD_LSTICK_RIGHT, 0},                  // 3  I_RIGHT
    {K_GAMEPAD_RSHOULDER, 0},                     // 4  I_THROW
    {K_GAMEPAD_LSHOULDER, 0},                     // 5  I_COMPORTEMENT
    {K_GAMEPAD_Y, 0},                             // 6  I_INVENTORY
    {K_GAMEPAD_X, 0},                             // 7  I_ACTION_M
    {K_GAMEPAD_A, 0},                             // 8  I_ACTION_ALWAYS
    {K_GAMEPAD_B, 0},                             // 9  I_RETURN
    {K_GAMEPAD_START, 0},                         // 10 I_MENUS
    {K_GAMEPAD_BACK, 0},                          // 11 I_HOLOMAP
    {0, 0},                                       // 12 I_PAUSE (unbound; Start opens the in-game menu)
    {K_GAMEPAD_LSTICK_BTN, K_GAMEPAD_RSTICK_BTN}, // 13 I_ESQUIVE
    {0, 0},                                       // 14 I_NORMAL
    {0, 0},                                       // 15 I_SPORTIF
    {0, 0},                                       // 16 I_AGRESSIF
    {0, 0},                                       // 17 I_DISCRET
    {0, 0},                                       // 18 I_HELP
    {0, 0},                                       // 19 I_SAVE
    {0, 0},                                       // 20 I_LOAD
    {0, 0},                                       // 21 I_OPTIONS
    {0, 0},                                       // 22 I_CAMERA (unbound; turn-camera pending auto-camera integration)
    {K_GAMEPAD_RSTICK_UP, 0},                     // 23 I_CAMERA_LEVEL_PLUS
    {K_GAMEPAD_RSTICK_DOWN, 0},                   // 24 I_CAMERA_LEVEL_MOINS
    {0, 0},                                       // 25 I_WEAPON_1
    {0, 0},                                       // 26
    {0, 0},                                       // 27
    {0, 0},                                       // 28
    {0, 0},                                       // 29
    {0, 0},                                       // 30
    {0, 0},                                       // 31
    {K_GAMEPAD_DPAD_RIGHT, 0},                    // 32 I_PINGOUIN
    {K_GAMEPAD_DPAD_DOWN, 0},                     // 33 I_JETPACK
    {K_GAMEPAD_DPAD_UP, 0},                       // 34 I_PROTECTION
    {K_GAMEPAD_DPAD_LEFT, 0},                     // 35 I_FOUDRE
};

/*══════════════════════════════════════════════════════════════════════════*
			██████┐ ██┐ ██┐ ██████┐ ██████┐
			└─██┌─┘ ███┐██│ └─██┌─┘ └─██┌─┘
			  ██│   ██████│   ██│     ██│
			  ██│   ██┌███│   ██│     ██│
			██████┐ ██│└██│ ██████┐   ██│
			└─────┘ └─┘ └─┘ └─────┘   └─┘
 *══════════════════════════════════════════════════════════════════════════*/

//-------------------------------------------------------------------------
void InitInput(void) {
    U32 mask;

    for (NbInput = 0; NbInput < MAX_INPUT * 2;) {
        mask = (1 << (NbInput / 2));

        TabInputBit[NbInput++] = mask;
        TabInputBit[NbInput++] = mask;
    }

    if (NbInput > 64)
        NbInput = 64;

    DefineInputKeys(NbInput, (U32 *)DefKeys, TabInputBit);
}

//-------------------------------------------------------------------------
void RestoreInput() {
    T_DEF_KEY *DEFKEYSDEFAULT;
    DEFKEYSDEFAULT = DefKeysDefault95;

    memcpy(DefKeys, DEFKEYSDEFAULT, sizeof(T_DEF_KEY) * MAX_INPUT);
    memcpy(GamepadKeys, GamepadKeysDefault, sizeof(GamepadKeys));
}

//-------------------------------------------------------------------------
void ReadInputConfig(void) {
    S32 initkeys = FALSE;
    S32 winmode;
    S32 n;
    char id[50];

    winmode = DefFileBufferReadValueDefault("WinMode", FALSE);

    if (!winmode) {
        RestoreInput();
    } else {
        T_DEF_KEY *DEFKEYSDEFAULT;
        DEFKEYSDEFAULT = DefKeysDefault95;

        // On peut identifier chaque input avec un tableau de chaine,
        // mais bon, bof !!!

        for (n = 0; n < MAX_INPUT; n++) {
            snprintf(id, sizeof(id), "Input%d_1", n);
            DefKeys[n].Key1 = DefFileBufferReadValueDefault(id, DEFKEYSDEFAULT[n].Key1);
            // Blindage si les Inputs sont configurés à 0
            //			if( !DefKeys[n].Key1 )	DefKeys[n].Key1 = DEFKEYSDEFAULT[n].Key1 ;

            snprintf(id, sizeof(id), "Input%d_2", n);
            DefKeys[n].Key2 = DefFileBufferReadValueDefault(id, DEFKEYSDEFAULT[n].Key2);
            // Blindage si les Inputs sont configurés à 0
            //			if( !DefKeys[n].Key2 )	DefKeys[n].Key2 = DEFKEYSDEFAULT[n].Key2 ;
        }
    }

    InitInput();
}

//-------------------------------------------------------------------------
void WriteInputConfig(void) {
    S32 n;
    char id[50];

    if (!FileSize(PathConfigFile))
        return;

    DefFileBufferWriteValue("WinMode", TRUE);

    // On peut identifier chaque input avec un tableau de chaine,
    // mais bon, bof !!!

    for (n = 0; n < MAX_INPUT; n++) {
        snprintf(id, sizeof(id), "Input%d_1", n);
        DefFileBufferWriteValue(id, DefKeys[n].Key1);

        snprintf(id, sizeof(id), "Input%d_2", n);
        DefFileBufferWriteValue(id, DefKeys[n].Key2);
    }
}

//-------------------------------------------------------------------------
void ReadGamepadConfig(void) {
    JoystickDeadzone = DefFileBufferReadValueDefault("GamepadDeadzone", 8000);
    if (JoystickDeadzone < 0)
        JoystickDeadzone = 0;
    if (JoystickDeadzone > 32767)
        JoystickDeadzone = 32767;

    char id[32];
    for (S32 n = 0; n < MAX_INPUT; n++) {
        snprintf(id, sizeof(id), "Gamepad%d", n);
        GamepadKeys[n].Key1 =
            DefFileBufferReadValueDefault(id, GamepadKeysDefault[n].Key1);
        snprintf(id, sizeof(id), "Gamepad%d_2", n);
        GamepadKeys[n].Key2 =
            DefFileBufferReadValueDefault(id, GamepadKeysDefault[n].Key2);
    }
}

//-------------------------------------------------------------------------
void WriteGamepadConfig(void) {
    if (!FileSize(PathConfigFile))
        return;

    DefFileBufferWriteValue("GamepadDeadzone", JoystickDeadzone);

    char id[32];
    for (S32 n = 0; n < MAX_INPUT; n++) {
        snprintf(id, sizeof(id), "Gamepad%d", n);
        DefFileBufferWriteValue(id, GamepadKeys[n].Key1);
        snprintf(id, sizeof(id), "Gamepad%d_2", n);
        DefFileBufferWriteValue(id, GamepadKeys[n].Key2);
    }
}

//-------------------------------------------------------------------------
// OR gamepad-bound contributions into the global Input bitfield. Only the
// first 32 action slots map to bits in Input (matching the NbInput=64 clamp
// applied by GetInput()); slots 32-35 are reserved for spells that the
// engine never reads through the Input path.
void ApplyGamepadBindings(void) {
    if (!JoystickIsPresent())
        return;
    for (S32 n = 0; n < 32; n++) {
        U32 k1 = GamepadKeys[n].Key1;
        U32 k2 = GamepadKeys[n].Key2;
        if ((k1 != 0 && CheckKey(k1)) || (k2 != 0 && CheckKey(k2))) {
            Input |= (1u << n);
        }
    }
}

//-------------------------------------------------------------------------
void MyGetInput() {
    GetJoys((U32 *)&TabKeys[256]);
    GetInput(0); // no norepeat keys
    ApplyGamepadBindings();

    MyKey = Key;
    // Gamepad "press any key" parity: if no keyboard key was pressed this
    // frame, expose the first newly-pressed gamepad scancode through MyKey so
    // intro/logo skips, WaitInput(), and menu prompts all respond to pad input
    // without needing to touch each callsite. Gamepad scancodes are >= 1024 so
    // they can't collide with existing MyKey == K_FOO keyboard branches.
    if (!MyKey) {
        MyKey = (S32)JoystickFirstPressedScancode();
    }

    /* Reserve configured console toggle key from gameplay so toggle isn't handled twice. */
    {
        int toggle_scancode = Console_GetToggleScancode();
        if (toggle_scancode > 0 && toggle_scancode < TABKEYS_NUM_KEYS)
            TabKeys[(U32)toggle_scancode] = 0;
        if (Key == toggle_scancode)
            Key = 0;
    }
    /* When we just closed the console, suppress all game input for one frame so the toggle key doesn't trigger camera etc. */
    if (Console_ShouldSuppressInputThisFrame()) {
        Key = 0;
        Input = 0;
        MyKey = 0;
        /* Clear I_CAMERA keys (index 22 per INPUT.H) so they don't fire on the suppress frame. */
        TabKeys[DefKeys[22].Key1] = 0;
        TabKeys[DefKeys[22].Key2] = 0;
        return;
    }
    if (Console_IsOpen()) {
        Console_Update();
        /* While the console is open, completely isolate game input:
		   no keys or joystick bits should be visible to gameplay or debug tools. */
        Key = 0;
        MyKey = 0;
        Input = 0;
        memset(TabKeys, 0, sizeof(TabKeys));
#ifdef LBA_EDITOR
        MyClick = 0;
#endif
        /* Force present so console updates (keypress, Enter/new line) are visible every frame. */
        BoxStaticAdd(0, 0, (S32)(ModeDesiredX - 1), (S32)(ModeDesiredY - 1));
        BoxUpdate();
        return;
    }

    // Attente d'événement de touche dans la boucle

    if (WaitNoKey) {
        if (MyKey) {
            MyKey = 0;
        } else
            WaitNoKey = 0;
    }

#ifdef LBA_EDITOR
    MyClick = Click;

    if (WaitNoClick) {
        if (MyClick & WaitNoClick) {
            MyClick &= ~(WaitNoClick);
        } else
            WaitNoClick = 0;
    }
#endif
}

//***************************************************************************
/*──────────────────────────────────────────────────────────────────────────*/
void WaitNoInput(void) {
    MyGetInput();

    while (MyKey OR Input
#ifdef LBA_EDITOR
               OR MyClick
#endif
    )
        MyGetInput();
}

/*──────────────────────────────────────────────────────────────────────────*/
void WaitInput(void) {
    MyGetInput();

    while (!MyKey AND !Input
#ifdef LBA_EDITOR
               AND !MyClick
#endif
    )
        MyGetInput();
}
//***************************************************************************
