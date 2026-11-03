#include "C_EXTERN.H"
#include "CONFIG.H"
#include "DIRECTORIES.H"
#include "GAMEMENU.H"
#include "GAMEMENU_CAPTURE.H"
#include "GAMEMENU_INTERNAL.H"
#include "GAMEMENU_MOUSE.H"
#include "IMAGE_LOAD.H"
#include "INPUT.H"
#include "MESSAGE.H"
#include "PERSO.H"
#include "PLAYACF.H"

#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>

/* Boot main menu (Resume / New Game / Load / Save / Options / Quit), its
 * runtime filtering, and the generic choice prompt. GameAskChoice shares the
 * page machinery but is asked from the game loop, not from the boot menu. */

#define MAX_MAIN_MENUS 6

U16 GameMainMenu[MENU_HEADER_WORDS + MAX_MAIN_MENUS * 2];

// Menu Principal Complet permettant de construire GameMainMenu
static const struct MenuRowDef kMainMenuRows[] = {
    {0, MENU_ID_MAIN_RESUME},  // reprendre partie
    {0, MENU_ID_MAIN_NEW},     // nouvelle partie
    {0, MENU_ID_MAIN_LOAD},    // charger partie
    {0, MENU_ID_MAIN_SAVE},    // sauver partie
    {0, MENU_ID_MAIN_OPTIONS}, // Options
    {0, MENU_ID_MAIN_QUIT},    // Quitter
};

static const struct MenuRowDef kGameChoiceRows[] = {
    {0, 0},
    {0, 1},
    {0, 2},
    {0, 3},
    {0, 4},
    {0, 5},
    {0, 6},
};
static U16 GameChoiceMenu[MENU_HEADER_WORDS + MENU_ROW_COUNT(kGameChoiceRows) * 2];

/* Harness one-shot capture of the boot-time main game menu -- the screen a
   player sees at launch with Resume / New Game / Load / Options / Quit.
   The normal entry point MainGameMenu() runs a while-loop with case-driven
   side effects (Credits, PlayAcf, save-game zoom animations) that we don't
   want under the harness; instead, replicate just the minimal setup
   MainGameMenu does before its DoGameMenu(GameMainMenu) call, arm the
   capture, and call DoGameMenu directly.  The capture site inside
   DoGameMenu (added for ui menu-options) fires and returns 1000, so this
   function returns cleanly without entering MainGameMenu's outer loop. */
void Menu_MainCapture(const char *path) {
    char tmpFilePathScreen[ADELINE_MAX_PATH];

    /* Setup mirrors MainGameMenu() lines 3373-3416 minus the audio start
       (CD music) and minus the goto-load_game / firstloop logic.  All the
       visual setup (background bitmap, plasma, palette, menu data, dial bank)
       runs so the captured frame matches what a player sees at launch. */
    HQ_StopSample();
    StopMusic();
    InitPlasmaMenu();
    FlagShadeMenu = FALSE;

    GetResPath(tmpFilePathScreen, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
    Image_LoadStretched(tmpFilePathScreen, Screen, PCR_MENU);
    CopyScreen(Screen, Log);
    BoxStaticFullflip();
    PtrPal = PtrPalNormal;
    PaletteSync(PtrPalNormal);

    BuildGameMainMenu(TRUE);
    InitDial(0);

    Menu_RequestCapture(path);
    DoGameMenu(GameMainMenu);
    Menu_RequestCapture(NULL); /* disarm if DoGameMenu somehow exited without firing it */
}

static void ShowSaveConfirmation(enum MenuLocalizedLabel label) {
    HQ_MixSample(SAMPLE_FOUND_OBJ, 0x1000, 0, 1, 64, 127);
    CopyScreen(Screen, Log);
    ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
    {
        char savedMsg[256];
        CopyUtf8ToCp850(savedMsg, sizeof savedMsg, GetLocalizedMenuLabel(label));
        DrawSingleString((S32)ModeDesiredX / 2, (S32)ModeDesiredY / 2, savedMsg);
    }
    BoxStaticFullflip();
    {
        S32 timer;
        ManageTime();
        timer = TimerRefHR + 1000;
        while (TimerRefHR <= timer AND !MyKey
                                 AND !(Input & (I_FIRE | I_ACTION | I_MENUS))) {
            ManageTime();
            MyGetInput();
        }
    }
    InitWaitNoInput(I_FIRE | I_ACTION | I_MENUS);
}

// Non-static entry so the in-game quick-save (PERSO.CPP) can show the same
// "Game Saved" chime + overlay as the main-menu Save path. Keeps the localized
// label enum internal to this module.
void ShowGameSavedConfirmation(void) {
    ShowSaveConfirmation(MENU_LABEL_GAME_SAVED);
}

void BuildGameMainMenu(S32 firstloop) {
    char memoplayername[MAX_SIZE_PLAYER_NAME + 1];
    char memogamepathname[ADELINE_MAX_PATH];
    char tmpSavePath[ADELINE_MAX_PATH];
    S32 nb = 0;
    U16 ycenter = 275;

    strcpy(memoplayername, PlayerName);
    strcpy(memogamepathname, GamePathname);

    // reprendre partie
    GetSavePath(tmpSavePath, ADELINE_MAX_PATH, CURRENTSAVE_FILENAME);
    if (ExistsFileOrDir(tmpSavePath)) {
        MenuSetRow(GameMainMenu, nb++, kMainMenuRows[0].type, kMainMenuRows[0].id);
        ycenter = 335;
    }

    // nouvelle partie
    MenuSetRow(GameMainMenu, nb++, kMainMenuRows[1].type, kMainMenuRows[1].id);

    // charger partie
    if (IsExistSavedGame())
        MenuSetRow(GameMainMenu, nb++, kMainMenuRows[2].type, kMainMenuRows[2].id);

    // sauver partie
    if (!firstloop AND SavingEnable)
        MenuSetRow(GameMainMenu, nb++, kMainMenuRows[3].type, kMainMenuRows[3].id);

    // Options
    MenuSetRow(GameMainMenu, nb++, kMainMenuRows[4].type, kMainMenuRows[4].id);

    // Quitter
    MenuSetRow(GameMainMenu, nb++, kMainMenuRows[5].type, kMainMenuRows[5].id);

    /* The template resets the cursor to the top on every rebuild; the
       firstloop == 2 path is the one exception (it lands on Quit). */
    GameMainMenu[0] = (U16)((firstloop == 2) ? nb - 1 : 0);
    MenuSetCount(GameMainMenu, nb);
    GameMainMenu[2] = ycenter;
    GameMainMenu[3] = 0;

    strcpy(PlayerName, memoplayername);
    strcpy(GamePathname, memogamepathname);
}

/*──────────────────────────────────────────────────────────────────────────*/

S32 GameInProgress = FALSE;

S32 MainGameMenu(S32 load) {
    S32 select;
    S32 flag = 0;
    S32 firstloop = TRUE;
    S32 savenewcube;
    S32 gogame = FALSE;
    S32 ret;
    char tmpFilePathSave[ADELINE_MAX_PATH];
    char tmpFilePathScreen[ADELINE_MAX_PATH];

    Log_Info("MainGameMenu: entered, load=%d", load);
    HQ_StopSample();
    StopMusic(); // Pour Stopper la music du Logo
    Log_Info("MainGameMenu: music stopped");

    InitPlasmaMenu();
    Log_Info("MainGameMenu: plasma menu initialized");
    FlagShadeMenu = FALSE;

#ifdef DEMO
    GetSavePath(tmpFilePathSave, ADELINE_MAX_PATH, CURRENTSAVE_FILENAME);
    if (!ExistsFileOrDir(tmpFilePathSave)
#ifdef DEBUG_TOOLS
            OR load
#endif
    ) {
#ifndef DEBUG_TOOLS
        load = 0;
#endif
        FlagBlackPal = FALSE;
        SetDemoSaveGame(load);
        goto load_game;
    }
#else
    if (IsFirstGameLaunched()) {
        FlagPlayAcf = TRUE;
        goto new_game;
    }
#endif

#ifndef DEMO
    if (load)
        goto load_game;
#endif

    GetResPath(tmpFilePathScreen, ADELINE_MAX_PATH, SCREEN_HQR_NAME);
    Image_LoadStretched(tmpFilePathScreen, Screen, PCR_MENU);
    CopyScreen(Screen, Log);
    BoxStaticFullflip();
    FadeToPal(PtrPalNormal);

    PtrPal = PtrPalNormal;

    BackupScreen(FALSE);
    CopyScreen(Log, Screen);

    MenuMouseAcquire();

    while (!flag) {
        if (gogame) {
            DemoSlide = FALSE;
            GameInProgress = FALSE;

            ClearPlasmaMenu(); // vire plasma menu

            FlagShadeMenu = TRUE;

            /* Menu mouse is for menu UI only; do not composite it during MainLoop (gameplay). */
            MenuMouseRelease();

            ret = MainLoop();

            BackupScreen(FALSE);

            switch (ret) {
            case 0: // <ESC>
                if (SavingEnable)
                    CurrentSaveGame(); // sauve dans current.lba
                break;

            case 1: // Gagné !!!!
#ifdef DEMO
                SlideShow();
                //                                        DemoBumper() ;
                GetSavePath(tmpFilePathSave, ADELINE_MAX_PATH, CURRENTSAVE_FILENAME);
                unlink(tmpFilePathSave);
                FlagShowEndDemo = TRUE;

                Image_LoadStretched(tmpFilePathScreen, Log, PCR_MENU);
                CopyScreen(Log, Screen);
                BoxStaticAdd(0, 0, ModeDesiredX - 1, ModeDesiredY - 1);

                PtrPal = PtrPalNormal;
                if (!FlagBlackPal)
                    FadeToBlack(PtrPal);

                FadeMenu = TRUE;
#else
                Credits(TRUE);
                PlayAcf("BABY");
                Image_LoadStretched(tmpFilePathScreen, Screen, PCR_MENU);
                CopyScreen(Screen, Log);
                // PlayAcf now leaves the screen black (original contract). Reveal the
                // win screen with its palette, the same PtrPalNormal the menu is drawn
                // with above; without this the composited image would present black.
                PtrPal = PtrPalNormal;
                FadeToPal(PtrPalNormal);
                EscTimer = 0;
#endif
                break;

            case 2: // GameOver :(
                // Pour ne pouvoir ni reprendre la partie
                // ni la sauver
                firstloop = TRUE;
                EscTimer = 0;
                break;
            }

            CopyScreen(Log, Screen);
            BoxReset();

            MemoClipWindow();
            UnsetClipWindow();

            ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
            BoxStaticFullflip();

            if (!FadeMenu)
                PaletteSync(PtrPalNormal);

            HQ_PauseSamples();
            FlagShadeMenu = TRUE;
            gogame = FALSE;
            InitPlasmaMenu();

            MenuMouseAcquire();
        }

        BuildGameMainMenu(firstloop);

        InitDial(0); //     SYS

#ifdef CDROM
        PlayMusic(CD_TRACK_MENU, TRUE);
#endif

        HQ_StopSample();

        //              GetMultiText( 49, PleaseWait ) ;

        select = DoGameMenu(GameMainMenu);
        switch (select) // num mess
        {
        case MENU_RET_ESC: // <ESC>, on reprend la partie en cours
            if (firstloop) {
                firstloop = 2;
                break;
            }

        case MENU_ID_MAIN_RESUME: // reprendre partie
            if (!firstloop AND !GameInProgress AND !DemoSlide) {
                ChoicePalette();
                FlagPal = TRUE;
                InitDial(START_FILE_ISLAND + Island);
                HQ_ResumeSamples();

#ifdef CDROM
                StopMusic();
#endif

                RestoreClipWindow();
                FlagFade = FALSE;
                gogame = TRUE;
                RestoreTimer();
            } else {
                GetSavePath(tmpFilePathSave, ADELINE_MAX_PATH, CURRENTSAVE_FILENAME);
                if (ExistsFileOrDir(tmpFilePathSave)) {
                    char memoplayername[256];
                    char memogamepathname[256];

                    DemoSlide = FALSE;

                    strcpy(memoplayername, PlayerName);
                    strcpy(memogamepathname, GamePathname);

                    StopMusic();

                    firstloop = FALSE;
                    strcpy(PlayerName, CURRENTSAVE_NAME);
                    strcpy(GamePathname, tmpFilePathSave);
                    InitGame(-1);
                    ChangeCube();

#ifdef DEMO
                    // Grosse rustine pour savoir dans quelle
                    // sauvegarde on se trouve
                    switch (NumCube) {
                    case 183:
                    case 184:
                        CurrentDemoSave = 1;
                        break;

                    case 125:
                        CurrentDemoSave = 2;
                        break;

                    default:
                        CurrentDemoSave = 0;
                    }

                    NewDemoSave = CurrentDemoSave;
                    FlagShowEndDemo = FALSE;
#endif

                    CopyScreen(Log, ScreenAux);

                    RestoreTimer();
                    SaveTimer();
                    AffScene(AFF_ALL_NO_FLIP);
                    FixeCinemaMode(FALSE);
                    CopyScreen(Log, Screen);

                    // au cas ou le mec meurre avant de faire <ESC>
                    CurrentSaveGame();

                    BoxReset(); // pas de flip

                    // Zoom starts from the floated preview box (must match the
                    // SAVE_THUMB_X, 10 + UI_VCENTER_OFS draw site above).
                    ZoomSavedGame(SAVE_THUMB_X, 10 + UI_VCENTER_OFS);

                    HQ_ResumeSamples();
                    RestoreTimer();

                    FlagFade = FALSE;
                    RestartRainSample = TRUE;

                    strcpy(PlayerName, memoplayername);
                    strcpy(GamePathname, memogamepathname);

                    gogame = TRUE;
                    EscTimer = 0;
                }
            }
            break;

        case MENU_ID_MAIN_NEW: // newgame
#ifdef DEMO
            FlagShowEndDemo = FALSE;
            SetDemoSaveGame(0);
            goto load_game;
#else
        new_game:
            firstloop = FALSE;
            strcpy(PlayerName, "");
            PtrPal = PtrPalNormal;
            StopMusic();
            InitGame(1);
            Introduction();
            HQ_StopSample();
            HQ_ResumeSamples();
            FlagFade = TRUE;
            gogame = TRUE;
            EscTimer = 0;
            break;
#endif

        case MENU_ID_MAIN_LOAD: // load
            switch (ChoosePlayerName(41, FALSE, FALSE)) {
            case 0:
                CopyScreen(Screen, Log);
                if (FlagShadeMenu)
                    ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
                BoxStaticFullflip();
                break;

            default: // load game
            load_game:
                firstloop = FALSE;
                DemoSlide = FALSE;
                StopMusic();
                InitGame(-1);
                ChangeCube();

#ifdef DEMO
                TakeAllDarts();
#endif

                CopyScreen(Log, ScreenAux);

                RestoreTimer();
                SaveTimer();
                AffScene(AFF_ALL_NO_FLIP);
                FixeCinemaMode(FALSE);
                CopyScreen(Log, Screen);

                // au cas ou le mec meurre avant de faire <ESC>
                CurrentSaveGame();

                BoxReset(); // pas de flip

                ZoomSavedGame(SAVE_THUMB_X, SAVE_THUMB_Y);

                HQ_ResumeSamples();
                RestoreTimer();

                FlagFade = FALSE;

                gogame = TRUE;
                EscTimer = 0;
                break;
            }
            break;

        case MENU_ID_MAIN_SAVE: // Save Game
            if (!firstloop) {
                savenewcube = NewCube;

                if (ChoosePlayerName(42, FALSE, TRUE)) {
                    NumVersion = NUM_VERSION | SAVE_COMPRESS;
                    CopyScreen(Screen, Log);
                    if (FileSize(OldGamePathname))
                        Delete(OldGamePathname);
                    SaveGame(TRUE);
                    HQ_ResumeSamples();
                    UnsetClipWindow();

                    // Success feedback: chime + brief overlay, skippable.
                    // See ShowSaveConfirmation. Benefits keyboard, mouse,
                    // and gamepad flows equally — pre-PR, this was silent.
                    ShowSaveConfirmation(MENU_LABEL_GAME_SAVED);
                }

                CopyScreen(Screen, Log);

                ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
                NewCube = savenewcube;
                BoxStaticFullflip();
                RestoreTimer();
            }
            break;

        case MENU_ID_MAIN_OPTIONS: // Options
            CopyScreen(Screen, Log);
            BoxStaticFullflip();
            OptionsMenu(TRUE, MENU_ID_BACK);
            break;

        case MENU_ID_MAIN_QUIT: // quitter
            FadeToBlack(PtrPalNormal);
            Cls();
            BoxUpdate();
            flag = 1;
            break;

#ifdef DEBUG_TOOLS
        // Gestion des sauvegardes de bugs
        case 2000: // chargement d'un bug
            switch (ChoosePlayerName(41 | 0x80000000, FALSE, FALSE)) {
            case 0:
                CopyScreen(Screen, Log);
                if (FlagShadeMenu)
                    ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
                BoxStaticFullflip();
                break;

            default: // load game
                firstloop = FALSE;
                DemoSlide = FALSE;
                StopMusic();
                InitGame(-1);
                ChangeCube();

                CopyScreen(Log, ScreenAux);

                RestoreTimer();
                SaveTimer();
                AffScene(AFF_ALL_NO_FLIP);
                FixeCinemaMode(FALSE);
                CopyScreen(Log, Screen);

                // au cas ou le mec meurre avant de faire <ESC>
                CurrentSaveGame();

                BoxReset(); // pas de flip

                ZoomSavedGame(SAVE_THUMB_X, SAVE_THUMB_Y);

                HQ_ResumeSamples();
                RestoreTimer();

                FlagFade = FALSE;

                gogame = TRUE;
                EscTimer = 0;
                break;
            }
            break;

        case 2001: // sauvegarde d'un bug
            if (!firstloop) {
                savenewcube = NewCube;

                if (ChoosePlayerName(42 | 0x40000000, FALSE, TRUE)) {
                    NumVersion = NUM_VERSION | SAVE_COMPRESS;
                    CopyScreen(Screen, Log);
                    SaveGame(TRUE);
                    NewCube = savenewcube;
                    HQ_ResumeSamples();
                    ShowSaveConfirmation(MENU_LABEL_BUG_SAVED);
                }

                CopyScreen(Screen, Log);

                ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
                NewCube = savenewcube;
                BoxStaticFullflip();
                RestoreTimer();
            }
            break;
#endif
        }
    }
    MenuMouseRelease();
    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*
       █▀▀▀▀ █▀▀▀█ █▄ ▄█ █▀▀▀▀       █▀▀▀▀ █   █ █▀▀▀█  █    █▀▀▀▀ █▀▀▀▀
       ██ ▀█ ██▀▀█ ██▀ █ ██▀▀        ██    ██▀▀█ ██  █  ██   ██    ██▀▀
       ▀▀▀▀▀ ▀▀  ▀ ▀▀  ▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀▀▀▀ ▀▀  ▀ ▀▀▀▀▀  ▀▀   ▀▀▀▀▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/
/*──────────────────────────────────────────────────────────────────────────*/

void GameAskChoice(S32 nummess) {
    S32 n;

    GameChoiceMenu[0] = 0;
    MenuSetCount(GameChoiceMenu, GameNbChoices);
    GameChoiceMenu[2] = 0;
    GameChoiceMenu[3] = (U16)(START_FILE_ISLAND + Island);
    for (n = 0; n < GameNbChoices; n++)
        MenuSetRow(GameChoiceMenu, n, 0 /* plain button */, GameListChoice[n]);

    MyDial(nummess);

    InitPlasmaMenu();

    MenuMouseAcquire();

    if (DoGameMenu(GameChoiceMenu) != MENU_RET_ESC)
        GameChoice = GameListChoice[GameChoiceMenu[0]]; // ret num mess
    else {
        GameChoice = GameListChoice[0]; // ESC

        // redessine curseur
        GameChoiceMenu[0] = 0;
        DrawGameMenu(GameChoiceMenu, FALSE);
    }

    ClearPlasmaMenu();

    MenuMouseRelease();

    InitWaitNoKey();
    InitWaitNoInput(I_FIRE | I_ACTION | I_MENUS);

#ifdef CDROM
    NumObjSpeak = NUM_PERSO;
    Speak(GameChoice);
    MyGetInput();
    while (TestSpeak() AND(MyKey != K_ESC AND !(Input & I_MENUS)))
        MyGetInput(); // Wait until silence
    StopSpeak();      // Security
#endif
}
