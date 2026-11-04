#include "C_EXTERN.H"
#include "CONFIG.H"
#include "GAMEMENU.H"
#include "GAMEMENU_CAPTURE.H"
#include "GAMEMENU_INTERNAL.H"
#include "GAMEMENU_MOUSE.H"
#include "CONSOLE/CONSOLE.H"
#include "INPUT.H"
#include "JOYSTICK.H"
#include "MESSAGE.H"

#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/WINDOW.H>

/* Options pages: the top-level list and the Audio, Language, Gameplay,
 * Controls and Cheats sub-pages it opens. Row tables live next to the loop
 * that owns them; the top-level Back row is built per entry because its label
 * depends on the caller (previous menu vs. return to the game). */

U16 GameOptionMenu[MENU_HEADER_WORDS + 7 * 2];

// The Options page always opens on Back; the remaining rows are fixed and
// written by OptionsMenu, which also supplies the context-dependent Back
// label (26 = previous menu, 15 = return to the game).
static const struct MenuRowDef kOptionRows[] = {
    {0, MENU_ID_DISPLAY},
    {0, MENU_ID_SOUND_VOLUME},
    {0, MENU_ID_CHOOSE_LANGUAGE},
    {0, MENU_ID_GAMEPLAY},
    {0, MENU_ID_CONTROLLER},
    {0, MENU_ID_CHEATS},
};

// Controller submenu: split keyboard remap and gamepad remap under one parent
// so adding the pad doesn't clutter the top-level Options screen.
static const struct MenuRowDef kControllerRows[] = {
    {0, MENU_ID_BACK},
    {0, MENU_ID_KEYBOARD_CONFIG}, // existing keyboard remap screen
    {0, MENU_ID_GAMEPAD_CONFIG},
};
static U16 ControllerMenu[MENU_HEADER_WORDS + MENU_ROW_COUNT(kControllerRows) * 2];

// Cheat submenu: one row per classic typed cheat code, built from the shared
// cheat table in CHEATCOD.CPP so the menu cannot drift from the codes the game
// already accepts by typing. Row i carries id MENU_ID_CHEAT_BASE + i; the
// toggling cheats (Speed, God, Sendell) show their On/Off state in the label.
static U16 CheatMenu[MENU_HEADER_WORDS + (1 + CHEAT_MENU_MAX_ENTRIES) * 2];

static void BuildCheatMenuEntries(void) {
    S32 count = CheatGetCount();
    S32 n = 0;
    S32 i;

    if (count > CHEAT_MENU_MAX_ENTRIES)
        count = CHEAT_MENU_MAX_ENTRIES;
    CheatMenu[0] = 0;   // selected
    CheatMenu[2] = 260; // y center
    CheatMenu[3] = 0;   // .dia num
    MenuSetRow(CheatMenu, n++, 0, MENU_ID_BACK);
    for (i = 0; i < count; i++)
        MenuSetRow(CheatMenu, n++, 0, (U16)(MENU_ID_CHEAT_BASE + i));
    MenuSetCount(CheatMenu, n);
}

static const struct MenuRowDef kLanguageRows[] = {
    {0, MENU_ID_BACK},
    {MENU_TYPE_TEXT_LANGUAGE, MENU_ID_TEXT_LANGUAGE},
#ifdef CDROM
    {MENU_TYPE_VOICE_LANGUAGE, MENU_ID_VOICE_LANGUAGE},
#endif
};
static U16 LanguageMenu[MENU_HEADER_WORDS + MENU_ROW_COUNT(kLanguageRows) * 2];

static const struct MenuRowDef kGameplayRows[] = {
    {0, MENU_ID_BACK},
    {0, MENU_ID_FOLLOW_CAMERA},
    {0, MENU_ID_MOVIE_CAMERA},
    {0, MENU_ID_VIDEO_SIZE},
    {0, MENU_ID_DISPLAY_TEXT},
    {0, MENU_ID_MOVIE_UPSCALE},
};
static U16 GameplayMenu[MENU_HEADER_WORDS + MENU_ROW_COUNT(kGameplayRows) * 2];

#ifdef DEMO
static const struct MenuRowDef kVolumeRows[] = {
    {0, MENU_ID_BACK},
    {2, MENU_ID_VOLUME_SAMPLES}, // reglage volume samples
    {4, MENU_ID_VOLUME_MUSIC},   // reglage volume musiques (Jingles)
    {6, MENU_ID_GENERAL_VOLUME},
    {0, MENU_ID_STEREO_CHANNELS}, // reverse stereo channels
};
U16 VolumeMenu[MENU_HEADER_WORDS + MENU_ROW_COUNT(kVolumeRows) * 2];
#else
U16 *VolumeMenu;

static const struct MenuRowDef kVolumeVoiceRows[] = {
    // integere volume des voix
    {0, MENU_ID_BACK},
    {2, MENU_ID_VOLUME_SAMPLES}, // reglage volume samples
    {3, MENU_ID_VOLUME_VOICE},   // reglage volume voix
    {4, MENU_ID_VOLUME_MUSIC},   // reglage volume musiques (Jingles)
    {6, MENU_ID_GENERAL_VOLUME},
    {0, MENU_ID_STEREO_CHANNELS}, // reverse stereo channels
};

static const struct MenuRowDef kVolumeNoVoiceRows[] = {
    {0, MENU_ID_BACK},
    {2, MENU_ID_VOLUME_SAMPLES}, // reglage volume samples
    {4, MENU_ID_VOLUME_MUSIC},   // reglage volume musiques (Jingles)
    {6, MENU_ID_GENERAL_VOLUME},
    {0, MENU_ID_STEREO_CHANNELS}, // reverse stereo channels
};

static U16 VolumeMenuVoice[MENU_HEADER_WORDS + MENU_ROW_COUNT(kVolumeVoiceRows) * 2];
static U16 VolumeMenuNoVoice[MENU_HEADER_WORDS + MENU_ROW_COUNT(kVolumeNoVoiceRows) * 2];
#endif

void CycleTextLanguage(S32 step) {
    const S32 count = GetLanguageCount();

    if (count <= 0) {
        return;
    }

    Language += step;
    if (Language < 0)
        Language = count - 1;
    if (Language >= count)
        Language = 0;

    ReloadMultiTextFile(0);
}

#ifdef CDROM
static void ClampVoiceLanguage(void) {
    const S32 count = GetVoiceLanguageCount();

    if (LanguageCD < 0 OR LanguageCD >= count) {
        LanguageCD = 1;
    }
}

void CycleVoiceLanguage(S32 step) {
    const S32 count = GetVoiceLanguageCount();

    if (count <= 0) {
        return;
    }

    ClampVoiceLanguage();

    LanguageCD += step;
    if (LanguageCD < 0)
        LanguageCD = count - 1;
    if (LanguageCD >= count)
        LanguageCD = 0;

    FlagSpeak = SamplesEnable;
}
#endif

S32 GereVolumeMenu(void) {
    S32 select;
    S32 flag = 0;

    S32 wasPaused = (GetSamplesPausedCount() > 0);
    if (wasPaused)
        ResumeSamples();

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    }

#ifdef CDROM
    PlayMusic(CD_TRACK_MENU, TRUE);
#endif

#ifdef DEMO
    MenuBuildRows(VolumeMenu, 240, kVolumeRows, MENU_ROW_COUNT(kVolumeRows));
#else
    if (FlagSpeak) {
        MenuBuildRows(VolumeMenuVoice, 240, kVolumeVoiceRows, MENU_ROW_COUNT(kVolumeVoiceRows));
        VolumeMenu = VolumeMenuVoice; // integre volume des voix
    } else {
        MenuBuildRows(VolumeMenuNoVoice, 240, kVolumeNoVoiceRows, MENU_ROW_COUNT(kVolumeNoVoiceRows));
        VolumeMenu = VolumeMenuNoVoice; // pas de volume des voix
    }
#endif

    while (!flag) {
        select = DoGameMenu(VolumeMenu);

        switch (select) // num mess
        {
        case MENU_ID_BACK: // menu precedent
        case MENU_RET_ESC: // <ESC>
            flag = 1;
            break;

        case MENU_ID_STEREO_CHANNELS:
            ReverseStereo ^= 1;
            InverseStereoSample(ReverseStereo);
            break;
        }
    }

    SetVolumeJingle(JingleVolume);

    if (wasPaused)
        PauseSamples();

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    WaitNoInput();

    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

S32 GereLanguageMenu(void) {
    S32 select;
    S32 flag = 0;

#ifdef CDROM
    ClampVoiceLanguage();
#endif

    MenuBuildRows(LanguageMenu, 260, kLanguageRows, MENU_ROW_COUNT(kLanguageRows));

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    }

    while (!flag) {
        select = DoGameMenu(LanguageMenu);

        switch (select) {
        case MENU_ID_BACK:
        case MENU_RET_ESC:
            flag = 1;
            break;

        case MENU_ID_TEXT_LANGUAGE:
            CycleTextLanguage(1);
            break;

#ifdef CDROM
        case MENU_ID_VOICE_LANGUAGE:
            CycleVoiceLanguage(1);
            break;
#endif
        }
    }

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    WaitNoInput();

    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/* Harness one-shot capture of the Gameplay submenu. Same shape as
   Display_MainCapture: the pre-pass mirrors what OptionsMenu runs before
   GereGameplayMenu (plasma texture, Log copy, palette sync, SYS dial
   bank — the GetMultiText rows read their strings from it), then
   DoGameMenu runs with the capture armed so the first settled frame lands
   in the PNG and the loop exits. */
void Gameplay_MainCapture(const char *path) {
    HQ_StopSample();
    InitPlasmaMenu();

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);
    PtrPal = PtrPalNormal;
    PaletteSync(PtrPalNormal);
    InitDial(0);

    Menu_RequestCapture(path);
    DoGameMenu(GameplayMenu);
    Menu_RequestCapture(NULL); /* disarm if DoGameMenu somehow exited otherwise */
}

/* Harness one-shot capture of the Cheat submenu. Same shape as
   Gameplay_MainCapture: CheatMenu is rebuilt from the shared cheat table
   first so the rows always match it. */
void Cheat_MainCapture(const char *path) {
    HQ_StopSample();
    InitPlasmaMenu();

    BuildCheatMenuEntries();

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);
    PtrPal = PtrPalNormal;
    PaletteSync(PtrPalNormal);
    InitDial(0);

    Menu_RequestCapture(path);
    DoGameMenu(CheatMenu);
    Menu_RequestCapture(NULL); /* disarm if DoGameMenu somehow exited otherwise */
}

/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

S32 GereGameplayMenu(void) {
    S32 select;
    S32 flag = 0;

    MenuBuildRows(GameplayMenu, 260, kGameplayRows, MENU_ROW_COUNT(kGameplayRows));

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    }

    while (!flag) {
        select = DoGameMenu(GameplayMenu);

        switch (select) {
        case MENU_ID_BACK:
        case MENU_RET_ESC:
            flag = 1;
            break;

        case MENU_ID_MOVIE_CAMERA:
            AllCameras ^= 1;
            break;

        case MENU_ID_FOLLOW_CAMERA:
            /* One camera setting, three modes: the classic rig, the auto
             * follow camera, and the first-person view. The underlying
             * flags stay separate because the rest of the game reads them
             * directly; they are mutually exclusive here. */
            CycleCameraSetting();
            break;

        case MENU_ID_VIDEO_SIZE:
            VideoFullScreen ^= 1;
            break;

        case MENU_ID_DISPLAY_TEXT:
            if (FlagSpeak) {
                FlagDisplayText ^= 1;
            } else {
                FlagDisplayText = TRUE;
            }
            break;

        case MENU_ID_MOVIE_UPSCALE:
            MovieUpscale = (MovieUpscale + 1) % 3;
            WriteConfigFile();
            break;
        }
    }

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    WaitNoInput();

    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

S32 GereControllerMenu(void) {
    S32 select;
    S32 flag = 0;

    MenuBuildRows(ControllerMenu, 260, kControllerRows, MENU_ROW_COUNT(kControllerRows));

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    }

    while (!flag) {
        select = DoGameMenu(ControllerMenu);

        switch (select) {
        case MENU_ID_BACK:
        case MENU_RET_ESC:
            flag = 1;
            break;

        case MENU_ID_KEYBOARD_CONFIG: // Configuration Clavier (keyboard remap screen, source-guarded)
            MenuConfig();
            break;

        case MENU_ID_GAMEPAD_CONFIG:
            MenuGamepadConfig();
            break;
        }
    }

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    WaitNoInput();

    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

S32 GereCheatMenu(void) {
    S32 select;
    S32 flag = 0;

    BuildCheatMenuEntries();

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    }

    while (!flag) {
        select = DoGameMenu(CheatMenu);

        switch (select) {
        case MENU_ID_BACK:
        case MENU_RET_ESC:
            flag = 1;
            break;

        default:
            // Rows are laid out in cheat-table order: applying fires the same
            // path as typing the code (toast + sound), and the redraw shows
            // the new On/Off state on the toggling rows.
            if (select >= MENU_ID_CHEAT_BASE && select < MENU_ID_CHEAT_BASE + CheatGetCount())
                TryExecuteCheatByIndex(select - MENU_ID_CHEAT_BASE);
            break;
        }
    }

    CopyScreen(Screen, Log);
    BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

    WaitNoInput();

    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*/
/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

S32 OptionsMenu(S32 flagflip, S32 backId) {
    S32 select;
    S32 flag = 0;
    S32 i;

    /* Harness capture pre-pass.  The normal in-game ESC -> OptionsMenu path
       (PERSO.CPP:2412) does explicit setup before OptionsMenu that the
       harness skips when --exec calls OptionsMenu directly:

           StopSpeak()       -- terminate any active dialogue speech so its
                                portrait / strip does not bleed through the
                                menu (visible on saves with an active dialog).
           InitDial(0)       -- switch the dialogue text bank to SYS (the menu
                                strings live there).  Without this the menu's
                                first item -- "Volume du son" -- reads junk or
                                empty from whatever bank the save had loaded.
           InitPlasmaMenu()  -- initialise the plasma/fire-bar texture in
                                SkySeaTexture; otherwise the bar at the top of
                                the menu renders whatever junk was in that
                                texture region.
           AffScene(NO_FLIP) -- render the live scene to Screen so the menu's
                                ShadeBoxBlk has the player-visible background
                                to darken.  Mirrors the inventory pre-pass.

       All gated on capture-armed; non-harness callers see no behaviour
       change. */
    if (Menu_IsCaptureArmed()) {
        StopSpeak();
        InitDial(0);
        InitPlasmaMenu();
        if (Console_GetCaptureBlackBg()) {
            /* Cleanroom golden — menu panel composites over solid black. */
            memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
            memset(Screen, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        } else {
            AffScene(AFF_ALL_NO_FLIP);
        }
    }

    BackupScreen(TRUE);

    /*        CopyScreen( Log, Screen ) ;

	BoxReset() ;
*/
    if (FlagShadeMenu) {
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
        BoxStaticFullflip();
        PaletteSync(PtrPalNormal);
    }

    HQ_StopSample();
    RestartRainSample = TRUE;

#ifdef CDROM
    PlayMusic(CD_TRACK_MENU, TRUE);
#endif

    MenuSetRow(GameOptionMenu, 0, 0, (U16)backId);
    for (i = 0; i < MENU_ROW_COUNT(kOptionRows); i++)
        MenuSetRow(GameOptionMenu, 1 + i, kOptionRows[i].type, kOptionRows[i].id);
    MenuSetCount(GameOptionMenu, 1 + MENU_ROW_COUNT(kOptionRows));
    GameOptionMenu[2] = 260;
    GameOptionMenu[3] = 0;

    MenuMouseAcquire();

    while (!flag) {
        select = DoGameMenu(GameOptionMenu);

        switch (select) // num mess
        {
        case MENU_ID_RETURN_TO_GAME: // retour au jeu
        case MENU_ID_BACK:           // menu precedent
        case MENU_RET_ESC:           // <ESC>
            flag = 1;
            break;

        case MENU_ID_SOUND_VOLUME:
            GereVolumeMenu();
            break;

        case MENU_ID_CHOOSE_LANGUAGE:
            GereLanguageMenu();
            break;

        case MENU_ID_CONTROLLER:
            GereControllerMenu();
            break;

        case MENU_ID_GAMEPLAY:
            GereGameplayMenu();
            break;

        case MENU_ID_CHEATS:
            GereCheatMenu();
            break;

        case MENU_ID_DISPLAY:
            GereDisplayMenu();
            break;
        }
    }

    HQ_StopSample();
    SetDetailLevel(); // prise en compte des différentes options

    if (flagflip) {
        CopyScreen(Screen, Log);
        BoxStaticFullflip();
    } else {
        // on vient du jeu
        RestoreClipWindow();
        RestoreScreen();
    }

    WaitNoInput();

    MenuMouseRelease();
    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
