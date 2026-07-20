#include "C_EXTERN.H"

#include <stdio.h>

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>

#include "INPUT.H"

#include <SYSTEM/LOG.H>
#include "JOYSTICK.H"
#include "FOLLOWCAM_CFG.H"

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

            snprintf(id, sizeof(id), "Input%d_2", n);
            DefKeys[n].Key2 = DefFileBufferReadValueDefault(id, DEFKEYSDEFAULT[n].Key2);

            if (DefKeys[n].Key1 OR DefKeys[n].Key2)
                initkeys = TRUE;
        }

        /* Blindage si les Inputs sont configurés à 0.
           A cfg with no input bindings at all leaves the game unplayable: the
           menu still opens, because its key is not in this table, but nothing
           moves, and the remap screen needs the very bindings that are gone.

           Clearing every keyboard binding IS reachable from the remap screen,
           so an empty keyboard table alone is not proof of corruption -- a
           gamepad-only layout looks exactly like that. The pad bindings decide
           between the two, and they have to be read from the cfg here rather
           than from GamepadKeys: ReadGamepadConfig runs after this function, so
           the live table still holds whatever preceded it. */
        if (!initkeys) {
            for (n = 0; n < MAX_INPUT; n++) {
                snprintf(id, sizeof(id), "Gamepad%d", n);
                if (DefFileBufferReadValueDefault(id, 0)) {
                    initkeys = TRUE;
                    break;
                }
                snprintf(id, sizeof(id), "Gamepad%d_2", n);
                if (DefFileBufferReadValueDefault(id, 0)) {
                    initkeys = TRUE;
                    break;
                }
            }
        }

        if (!initkeys) {
            Log_Warn("Keyboard bindings in the config are all unset; "
                     "restoring the default layout.");
            RestoreInput();
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

    GamepadCameraAnalog = DefFileBufferReadValueDefault("GamepadCameraAnalog", TRUE);
    if (GamepadCameraAnalog != 0)
        GamepadCameraAnalog = TRUE;
    else
        GamepadCameraAnalog = FALSE;

    GamepadCameraSensX = DefFileBufferReadValueDefault("GamepadCameraSensX", GAMEPAD_CAM_SENS_DEFAULT);
    if (GamepadCameraSensX < 1)
        GamepadCameraSensX = 1;
    if (GamepadCameraSensX > 10)
        GamepadCameraSensX = 10;

    GamepadCameraSensY = DefFileBufferReadValueDefault("GamepadCameraSensY", GAMEPAD_CAM_SENS_DEFAULT);
    if (GamepadCameraSensY < 1)
        GamepadCameraSensY = 1;
    if (GamepadCameraSensY > 10)
        GamepadCameraSensY = 10;

    GamepadCameraInvertY = DefFileBufferReadValueDefault("GamepadCameraInvertY", FALSE);
    if (GamepadCameraInvertY != 0)
        GamepadCameraInvertY = TRUE;
    else
        GamepadCameraInvertY = FALSE;

    GamepadCamMaxBeta = DefFileBufferReadValueDefault("GamepadCamMaxBeta", GAMEPAD_CAM_BETA_MAX_DEFAULT);
    if (GamepadCamMaxBeta < 1)
        GamepadCamMaxBeta = 1;
    if (GamepadCamMaxBeta > 200)
        GamepadCamMaxBeta = 200;

    GamepadCamMaxAlpha = DefFileBufferReadValueDefault("GamepadCamMaxAlpha", GAMEPAD_CAM_ALPHA_MAX_DEFAULT);
    if (GamepadCamMaxAlpha < 1)
        GamepadCamMaxAlpha = 1;
    if (GamepadCamMaxAlpha > 100)
        GamepadCamMaxAlpha = 100;

    char id[32];
    for (S32 n = 0; n < MAX_INPUT; n++) {
        snprintf(id, sizeof(id), "Gamepad%d", n);
        GamepadKeys[n].Key1 =
            DefFileBufferReadValueDefault(id, GamepadKeysDefault[n].Key1);
        snprintf(id, sizeof(id), "Gamepad%d_2", n);
        GamepadKeys[n].Key2 =
            DefFileBufferReadValueDefault(id, GamepadKeysDefault[n].Key2);
    }

    // Fold the freshly-loaded gamepad bindings into the combined input table.
    // Boot order runs this after ReadInputConfig()'s InitInput(), so this is
    // the rebuild that actually captures the configured pad keys.
    InitInput();
}

//-------------------------------------------------------------------------
void WriteGamepadConfig(void) {
    if (!FileSize(PathConfigFile))
        return;

    DefFileBufferWriteValue("GamepadDeadzone", JoystickDeadzone);
    DefFileBufferWriteValue("GamepadCameraAnalog", GamepadCameraAnalog);
    DefFileBufferWriteValue("GamepadCameraSensX", GamepadCameraSensX);
    DefFileBufferWriteValue("GamepadCameraSensY", GamepadCameraSensY);
    DefFileBufferWriteValue("GamepadCameraInvertY", GamepadCameraInvertY);
    DefFileBufferWriteValue("GamepadCamMaxBeta", GamepadCamMaxBeta);
    DefFileBufferWriteValue("GamepadCamMaxAlpha", GamepadCamMaxAlpha);

    char id[32];
    for (S32 n = 0; n < MAX_INPUT; n++) {
        snprintf(id, sizeof(id), "Gamepad%d", n);
        DefFileBufferWriteValue(id, GamepadKeys[n].Key1);
        snprintf(id, sizeof(id), "Gamepad%d_2", n);
        DefFileBufferWriteValue(id, GamepadKeys[n].Key2);
    }
}

//-------------------------------------------------------------------------
void MyGetInput() {
    // GetJoys() writes the gamepad state into TabKeys; GetInput() then folds
    // both keyboard and gamepad into Input through the combined table built by
    // InitInput(), so the pad honours NoRepeatInput like the keyboard does.
    GetJoys((U32 *)&TabKeys[256]);
    GetInput(0); // no norepeat keys

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
