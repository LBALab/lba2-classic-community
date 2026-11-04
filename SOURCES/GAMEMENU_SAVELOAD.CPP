#include "C_EXTERN.H"
#include "CONFIG.H"
#include "DIRECTORIES.H"
#include "GAMEMENU.H"
#include "GAMEMENU_CAPTURE.H"
#include "GAMEMENU_INTERNAL.H"
#include "GAMEMENU_MOUSE.H"
#include "INPUT.H"
#include "MESSAGE.H"
#include "SAVENAME_VALIDATION.H"

#include <SVGA/SCREEN.H>
#include <SVGA/VIDEO.H>
#include <SYSTEM/ANDROID.H>
#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/KEYBOARD_KEYS.H>
#include <SYSTEM/LOG.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/OPENXR.H>
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* Save/load pages and the player-name entry they share.
 *
 * PlayerGameList / FindPlayerFile back the Load/Save slot list; DrawOneString
 * and InputPlayerName own the name editor and its caret; DeleteSavedGame and
 * the save-thumbnail helpers own the confirmation flow. The menu driver calls
 * in through GAMEMENU.H, so this file has no per-page dispatch of its own. */

// save players games

#define NB_CAR_H 14
#define NB_CAR_V 5

char GamePathname[ADELINE_MAX_PATH];
char OldGamePathname[ADELINE_MAX_PATH];
char PlayerName[MAX_SIZE_PLAYER_NAME + 1];

S32 SelectPlayer = 0;

static const struct MenuRowDef kSavedConfirmRows[] = {
    {0, MENU_ID_CANCEL},      // annuler
    {0, MENU_ID_DELETE_SAVE}, // détruire une sauvegarde
};
static U16 SavedConfirmMenu[MENU_HEADER_WORDS + MENU_ROW_COUNT(kSavedConfirmRows) * 2];

int my_sort_function(const void *a, const void *b) {
    return (strcasecmp(*(char **)a, *(char **)b));
}

/*──────────────────────────────────────────────────────────────────────────*/

S32 PlayerGameList(char **ptrlistname, char *listname, S32 flagnew) {
    struct dirent *file;
    DIR *dir;
    S32 wlong;
    char pathname[ADELINE_MAX_PATH];
    S32 nb = 0;
    S32 handle;
    char **startptrlist = ptrlistname;
    U8 wbyte;

    GetSavePath(pathname, ADELINE_MAX_PATH, NULL);
    dir = opendir(pathname);
    if (dir != NULL) {
        while ((file = readdir(dir)) != NULL) {
            if (ade_strcasestr(file->d_name, ".LBA") != NULL) {
                GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);

                struct stat st;
                stat(pathname, &st);
                if (S_ISREG(st.st_mode)) {
                    // On ne liste pas l'autosave ni la currentsave
                    // D'ailleurs ces sauvegardes pourraient avoir une autre
                    // extension que .LBA ce qui éviterait ce test !
                    if ((ade_strcasestr(pathname, AUTOSAVE_FILENAME) == NULL)
                            AND ade_strcasestr(pathname, CURRENTSAVE_FILENAME) == NULL) {
                        // BUG WORKAROUND!
                        // Take the path again because, for some reason, on Wine the call to
                        //   stat() changes the disk letter on the path
                        //   passed (even parameter being a constant). Possibly a Bug on
                        //   WINE or/and MinGW libraries used.
                        GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
                        handle = OpenRead(pathname);
                        if (handle) {
                            Read(handle, &wbyte, 1);             // num version
                            Read(handle, &wlong, sizeof(wlong)); // num cube

                            *ptrlistname++ = listname;
                            do {
                                Read(handle, &wbyte, 1);
                                *listname++ = wbyte;
                            } while (wbyte != 0);

                            Close(handle);

                            nb++;
                            if (nb == MAX_PLAYER)
                                return nb;
                        }
                    }
                }
            }
        }
        closedir(dir);
    }

    qsort((void *)startptrlist, nb, sizeof(char *), my_sort_function);

    GetSavePath(pathname, ADELINE_MAX_PATH, AUTOSAVE_FILENAME);
    if (!flagnew AND nb < MAX_PLAYER AND ExistsFileOrDir(pathname)) {
        strcpy(GamePathname, pathname);
        LoadGamePlayerName();
        *ptrlistname = listname;
        strcpy(listname, PlayerName);
        nb++;
    }

    return nb;
}

/*──────────────────────────────────────────────────────────────────────────*/
#ifdef DEBUG_TOOLS
S32 BugGameList(char **ptrlistname, char *listname) {
    struct dirent *file;
    DIR *dir;
    S32 wlong;
    char pathname[ADELINE_MAX_PATH];
    S32 nb = 0;
    S32 handle;
    char **startptrlist = ptrlistname;
    U8 wbyte;

    GetBugPath(pathname, ADELINE_MAX_PATH, NULL);
    dir = opendir(pathname);
    if (dir != NULL) {
        while ((file = readdir(dir)) != NULL) {
            if (ade_strcasestr(file->d_name, ".LBA") != NULL) {
                GetBugPath(pathname, ADELINE_MAX_PATH, file->d_name);

                struct stat st;
                stat(pathname, &st);
                if (S_ISREG(st.st_mode)) {
                    GetBugPath(pathname, ADELINE_MAX_PATH, file->d_name);
                    handle = OpenRead(pathname);
                    if (handle) {
                        Read(handle, &wbyte, 1);             // num version
                        Read(handle, &wlong, sizeof(wlong)); // num cube

                        *ptrlistname++ = listname;
                        do {
                            Read(handle, &wbyte, 1);
                            *listname++ = wbyte;
                        } while (wbyte != 0);

                        Close(handle);

                        nb++;
                        if (nb == MAX_PLAYER)
                            return nb;
                    }
                }
            }
        }
        closedir(dir);
    }

    qsort((void *)startptrlist, nb, sizeof(char *), my_sort_function);

    return nb;
}

S32 FindBugFile(const char *bugname) {
    struct dirent *file;
    DIR *dir;
    char pathname[ADELINE_MAX_PATH];
    char playername[MAX_SIZE_PLAYER_NAME + 1];
    U8 *ptr;
    S32 handle;
    S32 wlong;
    U8 wbyte;

    GetBugPath(pathname, ADELINE_MAX_PATH, NULL);
    dir = opendir(pathname);
    if (dir != NULL) {
        while ((file = readdir(dir)) != NULL) {
            if (ade_strcasestr(file->d_name, ".LBA") != NULL) {
                GetBugPath(pathname, ADELINE_MAX_PATH, file->d_name);

                struct stat st;
                stat(pathname, &st);
                if (S_ISREG(st.st_mode)) {
                    handle = OpenRead(pathname);
                    Read(handle, &wbyte, 1);             // num version
                    Read(handle, &wlong, sizeof(wlong)); // num cube

                    ptr = (U8 *)playername;
                    do {
                        Read(handle, &wbyte, 1);
                        *ptr++ = wbyte;
                    } while (wbyte != 0);

                    Close(handle);

                    if (!strcasecmp(playername, bugname)) {
                        strcpy(GamePathname, pathname);
                        return TRUE;
                    }
                }
            }
        }
        closedir(dir);
    }

    return FALSE;
}
#endif

/*──────────────────────────────────────────────────────────────────────────*/

S32 FindPlayerFile() {
    struct dirent *file;
    DIR *dir;
    char pathname[ADELINE_MAX_PATH];
    char playername[MAX_SIZE_PLAYER_NAME + 1];
    U8 *ptr;
    S32 handle;
    S32 wlong;
    U8 wbyte;

    GetSavePath(pathname, ADELINE_MAX_PATH, NULL);
    dir = opendir(pathname);
    if (dir != NULL) {
        while ((file = readdir(dir)) != NULL) {
            if (ade_strcasestr(file->d_name, ".LBA") != NULL) {
                GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);

                struct stat st;
                stat(pathname, &st);
                if (S_ISREG(st.st_mode)) {
                    handle = OpenRead(pathname);
                    Read(handle, &wbyte, 1);             // num version
                    Read(handle, &wlong, sizeof(wlong)); // num cube

                    ptr = (U8 *)playername;
                    do {
                        Read(handle, &wbyte, 1);
                        *ptr++ = wbyte;

                    } while (wbyte != 0);

                    Close(handle);

                    if (!strcasecmp(PlayerName, playername)) {
                        strcpy(GamePathname, pathname);
                        return TRUE;
                    }
                }
            }
        }
        closedir(dir);
    }

    GetSavePath(pathname, ADELINE_MAX_PATH, AUTOSAVE_FILENAME);
    if (ExistsFileOrDir(pathname)) {
        strcpy(playername, PlayerName);

        strcpy(GamePathname, pathname);
        LoadGamePlayerName();

        if (!strcasecmp(PlayerName, playername)) {
            strcpy(PlayerName, playername);
            return TRUE;
        }

        strcpy(PlayerName, playername);
    }

    return FALSE;
}

/*══════════════════════════════════════════════════════════════════════════*
           █    ██▄ █ █▀▀▀█ █   █ ▀▀█▀▀       ██▄ █ █▀▀▀█ █▄ ▄█ █▀▀▀▀
           ██   ██▀██ ██▀▀▀ ██  █   ██        ██▀██ ██▀▀█ ██▀ █ ██▀▀
           ▀▀   ▀▀  ▀ ▀▀    ▀▀▀▀▀   ▀▀  ▀▀▀▀▀ ▀▀  ▀ ▀▀  ▀ ▀▀  ▀ ▀▀▀▀▀
 *══════════════════════════════════════════════════════════════════════════*/
/*──────────────────────────────────────────────────────────────────────────*/

/*──────────────────────────────────────────────────────────────────────────*/

#define DrawCursor() AffGraph(0, CursorX + 4, CursorY, HQR_Get(HQRPtrSprite, 9));

// Forward refs to marquee state defined below (DrawOneChoice's neighbourhood,
// ~line 1925). Reused here so the save-slot list can scroll overflowing names.
extern S32 WayChoice;
extern S32 DeltaChoice;
extern MOVE MoveChoice;
extern void InitShakeChoice(void);

void DrawOneString(S32 x, S32 y, char *playername, S32 draw) {
    S32 x0, y0, x1, y1;
    S32 dx;

    x0 = x - LargeurMenu / 2;
    x1 = x + LargeurMenu / 2;

    y0 = y - HAUTEUR_STANDARD / 2;
    y1 = y + HAUTEUR_STANDARD / 2;

    draw &= 0x0F;

    // fond

    BackupAngles(x0, y0, x1, y1);

    if (draw == 0) {
        CopyBlock(x0, y0, x1, y1, Screen, x0, y0, Log);
        ShadeBoxBlk(x0, y0, x1, y1, MENU_SHADE_LVL);
    }
    if (draw == 1) {
        DrawFire(y0, FIRE_INPUT_NAME);
    }
    if (draw == 2) {
        Box(x0, y0, x1, y1, 91);
    }

    // cadre
    DrawCadre(x0, y0, x1, y1, ALL_ANGLES);

    RestoreAngles(x0, y0, x1, y1);

    // text — clipped to the box so long names (e.g. scene-aware gamepad
    // saves "Désert de la Feuille Blanche - 2026-05-17 14-30-00") cannot
    // bleed past the frame. If the text overflows the box: selected slot
    // (draw == 1) ping-pong-scrolls via WayChoice/DeltaChoice/MoveChoice,
    // mirroring DrawOneChoice (line ~2090); unselected slot is left-aligned
    // and visually truncated. Caller is responsible for InitShakeChoice()
    // on selection change so the scroll restarts from the left edge.
    ColorFont(CoulTextMenu);
    MemoClip();
    SetClip(x0 + 1, y0, x1 - 1, y1);
    dx = SizeFont(playername);
    if (dx > LargeurMenu) {
        if (draw != 1) {
            Font(x0 + 8, y - 18, playername);
        } else {
            if (WayChoice == -1)
                DeltaChoice -= GetDeltaMove(&MoveChoice);
            else
                DeltaChoice += GetDeltaMove(&MoveChoice);
            if (DeltaChoice < LargeurMenu - dx - 8) {
                DeltaChoice = LargeurMenu - dx - 8;
                WayChoice = 1;
            }
            if (DeltaChoice > 8) {
                DeltaChoice = 8;
                WayChoice = -1;
            }
            Font(x0 + DeltaChoice, y - 18, playername);
        }
    } else {
        Font(x - dx / 2, y - 18, playername);
    }
    RestoreClip();

    if (CursorActif)
        DrawCursor();

    // flip

    BoxStaticAdd(x0, y0, x1, y1);
}

/*──────────────────────────────────────────────────────────────────────────*/

void ClearOneString(S32 x, S32 y) {
    S32 x0, y0, x1, y1;

    x0 = x - LargeurMenu / 2;
    x1 = x + LargeurMenu / 2;

    y0 = y - HAUTEUR_STANDARD / 2;
    y1 = y + HAUTEUR_STANDARD / 2;

    // fond
    CopyBlock(x0, y0, x1, y1, Screen, x0, y0, Log);
    if (FlagShadeMenu)
        ShadeBoxBlk(x0, y0, x1, y1, SCREEN_SHADE_LVL);
    BoxStaticAdd(x0, y0, x1, y1);
    BoxUpdate();
}

/*──────────────────────────────────────────────────────────────────────────*/

void DrawSingleString(S32 x, S32 y, char *string) {
    S32 x0, y0, x1, y1;
    S32 sf;

    sf = SizeFont(string);

    x0 = x - sf / 2;
    x1 = x + sf / 2;

    y0 = y - HAUTEUR_STANDARD / 2;
    y1 = y + HAUTEUR_STANDARD / 2;

    // text
    ColorFont(BLACK);
    Font(x0 + 4, y - 18 + 4, string);
    ColorFont(CoulTextMenu);
    Font(x0, y - 18, string);

    // flip
    BoxStaticAdd(x0, y0, x1, y1);
    BoxUpdate();
}

/*──────────────────────────────────────────────────────────────────────────*/

void DrawCadreNewGame(void) {
    S32 num = 16; // Logo Americain
    S32 dx, dy;
    S32 x, y;
    U8 *ptr;

    switch (DistribVersion) {
    case UNKNOWN_VERSION:
    case EA_VERSION:
        num = 11;
        break;
    }

    ptr = (U8 *)HQR_Get(HQRPtrSprite, num);

    GetDxDyGraph(0, &dx, &dy, ptr);

    DrawCadreSave(SAVE_THUMB_X, SAVE_THUMB_Y);

    x = (S32)ModeDesiredX / 2 - dx / 2;
    y = SAVE_THUMB_Y + (SCREEN_SAVE_HEIGHT) / 2 - dy / 2;

    AffGraph(0, x, y, ptr);
}

/*──────────────────────────────────────────────────────────────────────────*/
#define NB_GAME_CHOICE 5
// Save/load + name-entry slot list origin (also feeds the hit-test and the
// name-entry cursor). Authored on the 480-tall canvas; UI_VCENTER_OFS floats
// the list to the vertical middle on a taller framebuffer (no-op at 480).
#define Y_START_CHOICE (205 + UI_VCENTER_OFS)
#define GM_MENU_HALF_WIDTH_SAVE 275

static S32 GameMenuSaveListHitTest(U32 mx, U32 my, S32 nb, S32 start, S32 *outSel) {
    S32 n;
    S32 x0 = (S32)ModeDesiredX / 2 - GM_MENU_HALF_WIDTH_SAVE;
    S32 x1 = (S32)ModeDesiredX / 2 + GM_MENU_HALF_WIDTH_SAVE;

    for (n = 0; n < NB_GAME_CHOICE; n++) {
        S32 y;
        S32 y0;
        S32 y1;

        if (n + start >= nb)
            break;

        y = Y_START_CHOICE + 60 * n;
        y0 = y - HAUTEUR_STANDARD / 2;
        y1 = y + HAUTEUR_STANDARD / 2;
        if ((S32)mx >= x0 && (S32)mx <= x1 && (S32)my >= y0 && (S32)my <= y1) {
            *outSel = n + start;
            return 1;
        }
    }
    return 0;
}

// TODO(input): uses GetAscii() (keyboard-only) — non-keyboard users cannot enter
// save/profile names. They are routed past this function inside ChoosePlayerName:
// new-game slot gets a datetime stem, existing slot keeps its current name
// (overwrite). See the !LastInputWasKeyboard branch there.
S32 InputPlayerName(S32 choice, char *name) {
    char memoplayername[255];
    S32 flag = 1;
    S32 retval = FALSE;
    S32 akey;
    S32 memoasciimode = AsciiMode;
    S32 x = 0, y = 0;
    S32 len;
    S32 timeraff, timernoaff;

    AsciiMode = TRUE;
    while (GetAscii())
        ;

    strcpy(memoplayername, name);
    strcpy(PlayerName, name);

    FindPlayerFile();

    // pour pouvoir effacer une sauvegarde sous Windows si le joueur
    // change le nom de la sauvegarde
    if (strcasecmp(PlayerName, AUTOSAVE_NAME)        // on n'efface pas l'autosave !
        AND strcasecmp(PlayerName, CURRENTSAVE_NAME) // ni le currentsave !
        AND strcasecmp(PlayerName, "")
            AND(ade_strcasestr(GamePathname, AUTOSAVE_FILENAME) == NULL) // on n'efface pas l'autosave !
        AND(ade_strcasestr(GamePathname, CURRENTSAVE_FILENAME) == NULL)) // ni le currentsave !
    {
        strcpy(OldGamePathname, GamePathname);
    }

    len = strlen(PlayerName);

    x = (S32)ModeDesiredX / 2;
    y = Y_START_CHOICE + choice * 60;

    ManageTime();
    timeraff = TimerRefHR + 20 * 20;
    CursorActif = TRUE;

    while (flag != -1) {
        ManageTime();
        if (TimerRefHR < timeraff) {
            CursorX = x - 4;
            CursorY = y + 2;

            if (!len)
                CursorX -= 16;

            CursorActif = TRUE;
            timernoaff = TimerRefHR + 10 * 20;
        } else {
            if (TimerRefHR >= timernoaff) {
                timeraff = TimerRefHR + 20 * 20;
                CursorActif = TRUE;
                CursorX = x - 4;
                CursorY = y + 2;

                if (!len)
                    CursorX -= 16;
            } else
                CursorActif = FALSE;
        }

        DrawOneString((S32)ModeDesiredX / 2, y, PlayerName, 1);
        BoxUpdate();

        if (flag == 1) {
            x = (S32)ModeDesiredX / 2 + SizeFont(PlayerName) / 2;
            flag = 0;
        }

        do {
            akey = GetAscii();

            if (!IsValidChar(akey))
                akey = 0;

            switch (akey) {
            case A_ESC:
                strcpy(PlayerName, memoplayername);
                retval = FALSE;
                flag = -1;
                break;

            case A_RETURN:
                if (IsDatetimeReservedShape(PlayerName)) {
                    // Reserved shape: refuse and bounce back to the prior name.
                    PlayErrorSample();
                    strcpy(PlayerName, memoplayername);
                    len = strlen(PlayerName);
                    flag = 1;
                } else if (strcmp(PlayerName, "")
                               AND((FindPlayerFile() == FALSE)
                                       OR(!strcmp(PlayerName, memoplayername)))) {
                    retval = TRUE;
                    flag = -1;
                } else {
                    PlayErrorSample();
                    strcpy(PlayerName, memoplayername);
                    len = strlen(PlayerName);
                    flag = 1;
                }
                break;

            case A_LEFT:
            case A_BACKSPACE:
                if (len > 0) {
                    PlayerName[--len] = '\0';
                    flag = 1;
                }
                break;

            default:
                akey &= 0xFF;

                /* Max input cursor X = original 550 = centre(320) + 230 px. Keep the
                   same 230 px half-width at any resolution so the name-entry field
                   visible area stays at its authored 460 px width. */
                if (x < (S32)ModeDesiredX / 2 + 230 AND akey >= 32 AND akey <= 122) {
                    PlayerName[len++] = (char)akey;
                    PlayerName[len] = '\0';
                    x = (S32)ModeDesiredX / 2 + SizeFont(PlayerName) / 2;
                }
                break;
            }
            // end_switch
        } while (akey); // Tant que Buffer Clavier non vide
    }

    AsciiMode = memoasciimode;

    WaitNoInput();

    CursorActif = FALSE;

    return retval;
}

/*──────────────────────────────────────────────────────────────────────────*/
#define TEMPO_SCREEN_SAVE 110
#define TEMPO_REPEAT_KEY 150

S32 ChoosePlayerName(S32 mess, S32 flagflip, S32 flagnew) {
    S32 flag = 3;
    char *listplayername;
    char **ptrlist;
    S32 n, nb;
    S32 start = 0;
    S32 select = 0;
    S32 ys = Y_START_CHOICE;
    S32 retval = 0;
    char string[256];
    U8 *ptr;
    S32 timerzoom;
    S32 timeranotherkey;
    S32 padNavDir = 0;
    S32 timerdraw = 1;
    U32 draw = 1;
    U32 newcube = NewCube;
    S32 save_press = -1;
    S32 save_prev_click = 0;
    S32 mouse_fire = 0;
    char memoplayername[MAX_SIZE_PLAYER_NAME + 1];
#ifdef DEBUG_TOOLS
    S32 isBugMode = FALSE;
#endif

    strcpy(OldGamePathname, "");

    strcpy(memoplayername, PlayerName);

    CopyScreen(Screen, Log);

    if (FlagShadeMenu)
        ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);

#ifdef DEBUG_TOOLS
    if (mess & 0x80000000) // chargement d'un bug
    {
        CoulText(LBAWHITE, 0);
        GraphPrintf(FALSE, 0, 460, "Chargement d'un bug...");
        isBugMode = TRUE;
    }

    if (mess & 0x40000000) // sauvegarde d'un bug
    {
        CoulText(LBAWHITE, 0);
        GraphPrintf(FALSE, 0, 460, "Sauvegarde d'un bug...");
        isBugMode = TRUE;
    }

    mess &= 0x3FFFFFFF;
#endif

    BoxStaticFullflip();
    if (FlagShadeMenu)
        PaletteSync(PtrPalNormal);

    //      listplayername = (char *)Malloc( MAX_PLAYER * (MAX_SIZE_PLAYER_NAME+1) ) ;
    //      ptrlist = (char**)Malloc( MAX_PLAYER * sizeof(char*) ) ;

    listplayername = (char *)BufSpeak;
    ptrlist = (char **)(BufSpeak + MAX_PLAYER * (MAX_SIZE_PLAYER_NAME + 1));

    /*      if( !listplayername OR !ptrlist )
        {
                TheEnd( NOT_ENOUGH_MEM, "Choose Player Name" ) ;
	}
*/

#ifdef DEBUG_TOOLS
    if (isBugMode)
        nb = BugGameList(ptrlist, listplayername);
    else
#endif
        nb = PlayerGameList(ptrlist, listplayername, flagnew);

    strcpy(PlayerName, memoplayername);

    if (!nb AND !flagnew)
        return FALSE;

    if (flagnew AND nb < MAX_PLAYER - 1) {
        ptrlist[nb] = NewGameTxt;
        nb++;
    } else
        flagnew = 0;

    if (strcmp(PlayerName, "")) {
        for (select = 0; select < nb - 1; select++) {
            if (!strcasecmp(PlayerName, ptrlist[select]))
                break;
        }

        while (select >= (start + NB_GAME_CHOICE)) {
            if (start < (nb - 1)) {
                start++;
            }
        }
    }

    if (flagnew AND select == nb - 1)
        timerzoom = TimerRefHR;
    else
        timerzoom = 0;

    InitDial(0);

    InitWaitNoKey();
    padNavDir = JoystickMenuNavOnly();

    MenuMouseAcquire();
    save_press = -1;
    save_prev_click = 0;
    mouse_fire = 0;

    // Marquee state: starts left-aligned each time we enter the menu; reset
    // again on every selection change via the flag==1 redraw below.
    InitShakeChoice();

    while (MyKey != K_ESC AND !(Input & I_MENUS)) {
        ManageTime();

        if (flag == 3) {
            DrawSingleString((S32)ModeDesiredX / 2, 25 + UI_VCENTER_OFS, (char *)GetMultiText(mess, string));
            flag = 1;
        }

        if (flag == 1) {
            // Reset marquee so the newly-selected slot starts scrolling
            // from the left edge — matches DrawGameMenu's pattern.
            InitShakeChoice();
            for (n = 0; n < NB_GAME_CHOICE; n++) {
                if (n + start >= nb)
                    break;

                draw = ((n + start) == select);

                if ((n + start) == select) {
                    ys = Y_START_CHOICE + 60 * n;
                }

                if (start > 0 AND n == 0)
                    draw |= ARROW_SG;
                else if ((start + NB_GAME_CHOICE < nb) AND(n == NB_GAME_CHOICE - 1))
                    draw |= ARROW_ID;

                DrawOneString((S32)ModeDesiredX / 2, Y_START_CHOICE + 60 * n, ptrlist[n + start], draw);
            }

            draw = 1;

            if (start > 0 AND select == start)
                draw |= ARROW_SG;
            else if ((start + NB_GAME_CHOICE < nb) AND(select == start + NB_GAME_CHOICE - 1))
                draw |= ARROW_ID;

            /*                      while( Key )
                        {
                                DrawOneString( 320, ys, ptrlist[select], draw ) ;
                        }
*/
            timeranotherkey = TimerRefHR + TEMPO_REPEAT_KEY;
            Input = MyKey = 0;
            flag = 0;
        }

        // Per-frame: redraw the selected slot so the marquee scroll advances
        // on long names that don't fit the box (DrawOneString does the
        // overflow detection internally). The flag==1 path above only fires
        // on selection change; this keeps the animation alive in between.
        {
            S32 sn = select - start;
            if (sn >= 0 && sn < NB_GAME_CHOICE && (start + sn) < nb) {
                S32 sdraw = 1;
                if (start > 0 AND sn == 0)
                    sdraw |= ARROW_SG;
                else if ((start + NB_GAME_CHOICE < nb) AND(sn == NB_GAME_CHOICE - 1))
                    sdraw |= ARROW_ID;
                DrawOneString((S32)ModeDesiredX / 2, Y_START_CHOICE + 60 * sn, ptrlist[select], sdraw);
            }
        }

        if (timerdraw /*AND TimerRefHR>=timerdraw*/) {
            strcpy(PlayerName, ptrlist[select]);

            if (!flagnew OR select < nb - 1) {
#ifdef DEBUG_TOOLS
                if (isBugMode)
                    FindBugFile(PlayerName);
                else
#endif
                    FindPlayerFile();
                ptr = LoadGameScreen();
                if (ptr)
                    DrawScreenSave(ptr, SAVE_THUMB_X, SAVE_THUMB_Y);
            } else // Nouvelle partie
            {
                //                                DrawCadreSave( 240, 50 ) ;
                //				DrawSingleString( 320, 105, "New" ) ;

                DrawCadreNewGame();
            }

            timerdraw = 0;
        }

        if (FlagMenuMouse && MenuMouse_GetRefCount() > 0) {
            MyGetInput();
        } else if (TimerRefHR > timeranotherkey) {
            MyGetInput();
        }

        /* Suppress nav keys outside the repeat window; translate gamepad
           D-pad / stick to K_GRAY_UP/DOWN when the window is open.
           Edge-only for gamepad: fire on direction change so a deflected
           stick on menu entry does not auto-fire. */
        if (TimerRefHR <= timeranotherkey) {
            if (MyKey == K_GRAY_UP || MyKey == K_GRAY_DOWN)
                MyKey = 0;
            Input &= ~(I_UP | I_DOWN);
        } else if (!MyKey) {
            S32 jdir = JoystickMenuNavOnly();
            if (jdir != padNavDir) {
                padNavDir = jdir;
                if (jdir == K_UP)
                    MyKey = K_GRAY_UP;
                else if (jdir == K_DOWN)
                    MyKey = K_GRAY_DOWN;
            }
        }

        if (FlagMenuMouse && MenuMouse_GetRefCount() > 0) {
            S32 hitSel = -1;
            S32 cur, leftDown, prevLeft, leftDownEdge, leftUpEdge, relSel;
            S32 wy;
            bool saveMoved;

            GameMenuMousePollFocus();
            saveMoved = MouseConsumeMotionFlag();
            cur = Click;
            leftDown = (cur & 1) != 0;
            prevLeft = (save_prev_click & 1) != 0;
            leftDownEdge = leftDown && !prevLeft;
            leftUpEdge = !leftDown && prevLeft;
            save_prev_click = cur;

            if (saveMoved && GameMenuSaveListHitTest(MouseX, MouseY, nb, start, &hitSel)) {
                if (!leftDown && save_press < 0 && hitSel != select) {
                    select = hitSel;
                    flag = 1;
                    if (flagnew AND select == nb - 1)
                        timerzoom = TimerRefHR;
                    else
                        timerzoom = 0;
                    timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
                }
            }

            if (leftDownEdge && GameMenuSaveListHitTest(MouseX, MouseY, nb, start, &hitSel)) {
                save_press = hitSel;
                if (select != hitSel) {
                    select = hitSel;
                    flag = 1;
                    if (flagnew AND select == nb - 1)
                        timerzoom = TimerRefHR;
                    else
                        timerzoom = 0;
                    timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
                }
            }

            if (leftDown && save_press >= 0) {
                if (select != save_press) {
                    select = save_press;
                    flag = 1;
                }
            }

            if (leftUpEdge && save_press >= 0) {
                relSel = -1;
                if (GameMenuSaveListHitTest(MouseX, MouseY, nb, start, &relSel) && relSel == save_press)
                    mouse_fire = 1;
                save_press = -1;
            }

            wy = ConsumeMouseWheelY();
            while (wy > 0) {
                if (select < (nb - 1)) {
                    select++;
                    if (select >= (start + NB_GAME_CHOICE)) {
                        if (start < (nb - 1)) {
                            start++;
                        }
                    } else {
                        DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[select - 1], (draw & 0xFFFFFFFE));
                    }
                    flag = 1;

                    if (flagnew AND select == nb - 1)
                        timerzoom = TimerRefHR;
                    else
                        timerzoom = 0;
                    timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
                }
                wy--;
            }
            while (wy < 0) {
                if (select > 0) {
                    select--;

                    if (select < start) {
                        start--;
                    } else {
                        DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[select + 1], (draw & 0xFFFFFFFE));
                    }
                    flag = 1;
                    if (flagnew AND select == nb - 1)
                        timerzoom = TimerRefHR;
                    else
                        timerzoom = 0;
                    timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
                }
                wy++;
            }
        }

        DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[select], draw);
        BoxUpdate();

        if (Input & I_DOWN OR MyKey == K_GRAY_DOWN) {
            if (select < (nb - 1)) {
                select++;
                if (select >= (start + NB_GAME_CHOICE)) {
                    if (start < (nb - 1)) {
                        start++;
                    }
                } else {
                    DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[select - 1], (draw & 0xFFFFFFFE));
                }
                flag = 1;

                if (flagnew AND select == nb - 1)
                    timerzoom = TimerRefHR;
                else
                    timerzoom = 0;
                timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
            }
        }

        if (Input & I_UP OR MyKey == K_GRAY_UP) {
            if (select > 0) {
                select--;

                if (select < start) {
                    start--;
                } else {
                    DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[select + 1], (draw & 0xFFFFFFFE));
                }
                flag = 1;
                if (flagnew AND select == nb - 1)
                    timerzoom = TimerRefHR;
                else
                    timerzoom = 0;
                timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
            }
        }

        //              if( !Input )    // on s'assure qu'aucune des touches testées
        // ci-dessous ne fait partie des Inputs
        {
            if (MyKey == K_GRAY_PAGE_DOWN) {
                if (select < (nb - 1)) {
                    S32 oldselect = select;

                    select += NB_GAME_CHOICE;
                    if (select >= nb)
                        select = nb - 1;

                    if (select >= (start + NB_GAME_CHOICE)) {
                        start += NB_GAME_CHOICE;
                        if (start >= (nb - (NB_GAME_CHOICE - 1)))
                            start = nb - NB_GAME_CHOICE;
                    } else {
                        DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[oldselect], (draw & 0xFFFFFFFE));
                    }
                    flag = 1;

                    if (flagnew AND select == nb - 1)
                        timerzoom = TimerRefHR;
                    else
                        timerzoom = 0;
                    timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
                }
            }

            if (MyKey == K_GRAY_PAGE_UP) {
                if (select > 0) {
                    S32 oldselect = select;

                    select -= NB_GAME_CHOICE;
                    if (select < 0)
                        select = 0;

                    if (select < start) {
                        start = select;
                    } else {
                        DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[oldselect], (draw & 0xFFFFFFFE));
                    }
                    flag = 1;
                    if (flagnew AND select == nb - 1)
                        timerzoom = TimerRefHR;
                    else
                        timerzoom = 0;
                    timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
                }
            }

            if (MyKey == K_GRAY_HOME) {
                if (select > 0) {
                    S32 oldselect = select;

                    select = 0;

                    if (start == 0) {
                        DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[oldselect], (draw & 0xFFFFFFFE));
                    } else
                        start = 0;

                    flag = 1;
                    if (flagnew AND select == nb - 1)
                        timerzoom = TimerRefHR;
                    else
                        timerzoom = 0;
                    timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
                }
            }
        }

        if (MyKey == K_GRAY_END) {
            if (select < (nb - 1)) {
                S32 oldselect = select;

                select = nb - 1;

                if (start == (nb - NB_GAME_CHOICE)) {
                    DrawOneString((S32)ModeDesiredX / 2, ys, ptrlist[oldselect], (draw & 0xFFFFFFFE));
                } else {
                    start = nb - NB_GAME_CHOICE;
                    if (start < 0)
                        start = 0;
                }

                flag = 1;
                if (flagnew AND select == nb - 1)
                    timerzoom = TimerRefHR;
                else
                    timerzoom = 0;
                timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
            }
        }

        // Effacement d'une sauvegarde
        if (MyKey == K_D OR MyKey == K_BACKSPACE OR MyKey == K_GRAY_SUPPR) {
            CopyScreen(Screen, Log);
            if (FlagShadeMenu)
                ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
            BoxStaticFullflip();

            if (DeleteSavedGame()) {
                if (nb > 1 AND select < nb - 1) {
                    memcpy(&ptrlist[select], &ptrlist[select + 1], ((nb - 1) - select) * sizeof(char *));
                }

                // recharge list player
                nb--;

                if (select >= nb) {
                    select = nb - 1;
                    if (select < 0)
                        select = 0;

                    if (select < start) {
                        start = select;
                        if (start < 0)
                            start = 0;
                    }
                }

                if (!nb)
                    break; // plus de sauvegarde
            }

            CopyScreen(Screen, Log);
            if (FlagShadeMenu)
                ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
            BoxStaticFullflip();

            flag = 1; // redraw menu
            timerdraw = TimerRefHR + TEMPO_SCREEN_SAVE;
        }

        /* Quest A loads the selected save.  Keep it out of the save/new-game
           path: that screen keeps the existing confirm keys. */
        S32 loadFire = Input & I_FIRE;
        if (OpenXR_IsActive() && !flagnew)
            loadFire |= Input & I_ACTION_ALWAYS;

        if (loadFire OR MyKey == K_ENTER OR MyKey == K_NUMPAD_ENTER OR mouse_fire) {
            if (mouse_fire)
                mouse_fire = 0;
            if (timerdraw) {
                strcpy(PlayerName, ptrlist[select]);

                if (!flagnew OR select < nb - 1) {
#ifdef DEBUG_TOOLS
                    if (isBugMode)
                        FindBugFile(PlayerName);
                    else
#endif
                        FindPlayerFile();
                    ptr = LoadGameScreen();
                    if (ptr)
                        DrawScreenSave(ptr, SAVE_THUMB_X, SAVE_THUMB_Y);
                } else // Nouvelle partie
                {
                    //					DrawCadreSave( 240, 50 ) ;
                    //					DrawSingleString( 320, 105, "New" ) ;
                    DrawCadreNewGame();
                }

                timerdraw = 0;
            }

            if (flagnew) // Nouvelle partie
            {
                char memoname[ADELINE_MAX_PATH];

                if (select == nb - 1)
                    strcpy(PlayerName, "");
                else {
#ifdef DEBUG_TOOLS
                    if (isBugMode)
                        FindBugFile(PlayerName);
                    else
#endif
                        FindPlayerFile();
                    strcpy(memoname, GamePathname);
                }

                while ((Input & I_FIRE) OR(OpenXR_IsActive() && !flagnew &&
                                           (Input & I_ACTION_ALWAYS)) OR
                           MyKey == K_ENTER)
                    MyGetInput();

                // Non-keyboard routing: unless the last input was a physical
                // keyboard, the player has no practical way to type a name
                // (InputPlayerName is keyboard-only) — this covers gamepad,
                // touch, and mouse. Android TV is forced down this path too:
                // there is no text keyboard on a remote, and SDL's routing of
                // remote D-pad keys is device-dependent, so IsAndroidTVDevice()
                // makes the choice deterministic (it is a no-op stub off
                // Android, so desktop and phone are unaffected). For a "New
                // Game" slot we synthesise a local-time datetime stem; for an
                // existing slot we keep the slot's own name and just overwrite.
                // Same-second collision overwrites silently — matches the
                // quick-save reflex chosen during design. The menu UI (slot
                // list + New Game entry) is identical to the keyboard flow;
                // only the text-entry step is bypassed.
                if (!LastInputWasKeyboard || IsAndroidTVDevice()) {
                    S32 ok = TRUE;
                    if (select == nb - 1) {
                        char datetime[32];
                        time_t now = time(NULL);
                        struct tm *lt = localtime(&now);
                        if (lt == NULL || strftime(datetime, sizeof datetime,
                                                   "%Y-%m-%d %H-%M-%S", lt) != 19) {
                            ok = FALSE;
                        } else {
                            // Scene-aware name: "<island> - <datetime>", with
                            // bare datetime as fallback. Island names live in
                            // text file 2 (gam); MainGameMenu has file 0 (sys)
                            // loaded right now (InitDial(0) at ~line 3330), so
                            // bracket the lookup and restore sys after — the
                            // menu redraws on the next loop iteration assume
                            // sys is still loaded. TabArrow defaults to zeros
                            // before the player opens the holomap, so the
                            // Mess > 0 guard avoids querying message 0 (which
                            // is a real but unrelated entry in some banks).
                            char islandName[256];
                            islandName[0] = '\0';
                            if (Island >= 0 && Island < MAX_ISLAND && TabArrow[Island].Mess > 0) {
                                InitDial(2);
                                if (GetMultiText(TabArrow[Island].Mess,
                                                 islandName) != NULL) {
                                    // Holomap data ships with a trailing '.'
                                    // ("Citadel Island."); strip for a cleaner
                                    // save name.
                                    size_t ilen = strlen(islandName);
                                    if (ilen > 0 && islandName[ilen - 1] == '.') {
                                        islandName[ilen - 1] = '\0';
                                    }
                                }
                                InitDial(0);
                            }
                            if (islandName[0]) {
                                // %.60s caps the island portion so the format
                                // is provably bounded (60 + 3 + 19 = 82 chars,
                                // safely under MAX_SIZE_PLAYER_NAME = 100) and
                                // -Wformat-truncation stays quiet. Longest
                                // known retail island name is 29 chars.
                                snprintf(PlayerName, MAX_SIZE_PLAYER_NAME + 1,
                                         "%.60s - %.19s", islandName, datetime);
                            } else {
                                strncpy(PlayerName, datetime,
                                        MAX_SIZE_PLAYER_NAME);
                                PlayerName[MAX_SIZE_PLAYER_NAME] = '\0';
                            }
                        }
                    }
                    // Existing slot: PlayerName already holds the slot name
                    // from the strcpy earlier in this loop (around line 1660),
                    // so nothing to fill — just resolve the path.
                    if (ok) {
                        char tmpPlayerFilename[ADELINE_MAX_PATH];
                        snprintf(tmpPlayerFilename, ADELINE_MAX_PATH, "%s.LBA",
                                 PlayerName);
#ifdef DEBUG_TOOLS
                        if (isBugMode)
                            GetBugPath(GamePathname, ADELINE_MAX_PATH,
                                       tmpPlayerFilename);
                        else
#endif
                            GetSavePath(GamePathname, ADELINE_MAX_PATH,
                                        tmpPlayerFilename);
                        retval = 2;
                        break;
                    }
                    // strftime is effectively infallible on real time_t; if
                    // it does fail, fall through to the keyboard flow rather
                    // than write a garbage filename.
                }

                if (InputPlayerName(select - start, PlayerName)) {
                    char tmpPlayerFilename[ADELINE_MAX_PATH];
                    if (select == nb - 1) {
                        do {
                            snprintf(tmpPlayerFilename, ADELINE_MAX_PATH, "%s.LBA", PlayerName);
#ifdef DEBUG_TOOLS
                            if (isBugMode)
                                GetBugPath(GamePathname, ADELINE_MAX_PATH, tmpPlayerFilename);
                            else
#endif
                                GetSavePath(GamePathname, ADELINE_MAX_PATH, tmpPlayerFilename);
                        } while (FileSize(GamePathname) != 0); // TODO: Review this sorcery
                    } else {
                        // renommer une sauvegarde
                        snprintf(tmpPlayerFilename, ADELINE_MAX_PATH, "%s.LBA", PlayerName);
#ifdef DEBUG_TOOLS
                        if (isBugMode)
                            GetBugPath(GamePathname, ADELINE_MAX_PATH, tmpPlayerFilename);
                        else
#endif
                            GetSavePath(GamePathname, ADELINE_MAX_PATH, tmpPlayerFilename);
                    }

                    retval = 2;
                    break;
                } else {
                    while (Input & I_FIRE OR MyKey == K_ENTER)
                        MyGetInput();
                    if (select == nb - 1)
                        strcpy(PlayerName, NewGameTxt);
                    timerdraw = 1;
                    flag = 3; // Rafraichissement ecran
                }
            } else {
                retval = 1;
                break;
            }
        }
    }

    MenuMouseRelease();

    if (Key == K_ESC OR Input & I_MENUS) {
        NewCube = newcube;
        strcpy(PlayerName, memoplayername);
    } else if (retval == 1) {
        strcpy(PlayerName, ptrlist[select]);
#ifdef DEBUG_TOOLS
        if (isBugMode)
            FindBugFile(PlayerName);
        else
#endif
            FindPlayerFile();

        for (n = 0; n < NB_GAME_CHOICE; n++) {
            if ((n + start) != select) {
                ClearOneString((S32)ModeDesiredX / 2, Y_START_CHOICE + 60 * n);
            }
        }
    }

    //      Free( ptrlist ) ;
    //      Free( listplayername ) ;

    if (flagflip) {
        CopyScreen(Screen, Log);
        if (FlagShadeMenu)
            ShadeBoxBlk(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, SCREEN_SHADE_LVL);
        BoxStaticFullflip();
    }

    SelectPlayer = select - start;

    return retval;
}

////////************* TEMPO, devrait etre en LIB ?

S32 DeleteSavedGame(void) {
    U8 *ptr;
    S32 ret = -1;
    S32 select;

    // Delete-save confirmation: float the name + preview with SavedConfirmMenu
    // (which floats via DrawGameMenu) so the whole screen stays coherent. No-op at 480.
    DrawOneString((S32)ModeDesiredX / 2, 80 + UI_VCENTER_OFS, PlayerName, 2);
    ptr = LoadGameScreen();
    if (ptr)
        DrawScreenSave(ptr, SAVE_THUMB_X, 130 + UI_VCENTER_OFS);
    BoxUpdate();

    MenuBuildRows(SavedConfirmMenu, 350, kSavedConfirmRows, MENU_ROW_COUNT(kSavedConfirmRows));
    SavedConfirmMenu[0] = 0;

    MenuMouseAcquire();

    while (ret == -1) {
        select = DoGameMenu(SavedConfirmMenu);

        switch (select) // num mess
        {
        case MENU_ID_CANCEL: // annuler
        case MENU_RET_ESC:   // <ESC>
            ret = 0;
            break;

        case MENU_ID_DELETE_SAVE: // detruire
            // retourne 0 si Pb -> pas effacé
            //          1 si Ok
            ret = Delete(GamePathname);
            break;
        }
    }

    InitWaitNoKey();
    InitWaitNoInput(I_FIRE | I_ACTION | I_JOY | I_MENUS);

    MenuMouseRelease();
    return ret;
}

/*══════════════════════════════════════════════════════════════════════════*/

#define TIME_ZOOM_SAVE (30 * 20)

/*──────────────────────────────────────────────────────────────────────────*/
void ZoomSavedGame(S32 x, S32 y) {
    S32 x0 = x, y0 = y, x1 = x + SCREEN_SAVE_WIDTH - 1, y1 = y + SCREEN_SAVE_HEIGHT - 1;
    S32 time, savetimer;
    S32 flag = FALSE;

    ManageTime();
    savetimer = TimerRefHR;

    CopyScreen(ScreenAux, Log); // pour cleaner le Log

    while (!flag) {
        ManageTime();
        time = TimerRefHR - savetimer;

        x0 = RegleTrois(x, 0, TIME_ZOOM_SAVE, time);
        x1 = RegleTrois(x + SCREEN_SAVE_WIDTH - 1, (S32)ModeDesiredX - 1, TIME_ZOOM_SAVE, time);
        y0 = RegleTrois(y, 0, TIME_ZOOM_SAVE, time);
        y1 = RegleTrois(y + SCREEN_SAVE_HEIGHT - 1, (S32)ModeDesiredY - 1, TIME_ZOOM_SAVE, time);

        if (x0 <= 0 OR y0 <= 0 OR x1 >= (S32)ModeDesiredX - 1 OR y1 >= (S32)ModeDesiredY - 1) {
            flag = TRUE;
        } else {
            ScaleBox(0, 0, (S32)ModeDesiredX - 1, (S32)ModeDesiredY - 1, Screen, x0, y0, x1, y1, Log);
            BoxStaticAdd(x0, y0, x1, y1);
            BoxUpdate();
        }
    }

    CopyScreen(Screen, Log);
    PaletteSync(PtrPal);
    BoxStaticFullflip();
}

/*──────────────────────────────────────────────────────────────────────────*/
void DrawScreenSave(U8 *ptr, S32 x, S32 y) {
    S32 x1, y1;

    x1 = x + SCREEN_SAVE_WIDTH - 1;
    y1 = y + SCREEN_SAVE_HEIGHT - 1;

    BackupAngles(x - 1, y - 1, x1 + 1, y1 + 1);
    Box(x, y, x1, y1, 0);
    RestoreBlock(Log, ptr, x, y, x1, y1);
    DrawCadre(x - 1, y - 1, x1 + 1, y1 + 1, ALL_ANGLES);
    RestoreAngles(x - 1, y - 1, x1 + 1, y1 + 1);
    BoxStaticAdd(x - 1, y - 1, x1 + 1, y1 + 1);
    BoxUpdate();
}

/*──────────────────────────────────────────────────────────────────────────*/
void DrawCadreSave(S32 x, S32 y) {
    S32 x1, y1;

    x1 = x + SCREEN_SAVE_WIDTH - 1;
    y1 = y + SCREEN_SAVE_HEIGHT - 1;

    BackupAngles(x - 1, y - 1, x1 + 1, y1 + 1);
    Box(x, y, x1, y1, 0);
    DrawCadre(x - 1, y - 1, x1 + 1, y1 + 1, ALL_ANGLES);
    RestoreAngles(x - 1, y - 1, x1 + 1, y1 + 1);
    BoxStaticAdd(x - 1, y - 1, x1 + 1, y1 + 1);
    //        BoxUpdate() ;
}

/*──────────────────────────────────────────────────────────────────────────*/
S32 IsExistSavedGame(void) {
    // a optimiser
    return PlayerGameList((char **)BufSpeak, (char *)BufSpeak + 100000L, FALSE);
}

/*──────────────────────────────────────────────────────────────────────────*/
S32 FirstGame = FALSE;

S32 IsFirstGameLaunched(void) {
    char tmpSaveFilePath[ADELINE_MAX_PATH];
    if (!FirstGame) {
        GetSavePath(tmpSaveFilePath, ADELINE_MAX_PATH, CURRENTSAVE_FILENAME);
        if (!IsExistSavedGame()
                AND !ExistsFileOrDir(tmpSaveFilePath)) {
            FirstGame = TRUE;
        }
    }

    return FirstGame;
}

/*──────────────────────────────────────────────────────────────────────────*/
/*
   GameMainMenu:

        0, 70,  // reprendre partie
        0, 71,  // nouvelle partie
        0, 72,  // charger partie
        0, 73,  // sauver partie
        0, 74,  // Options
        0, 75,  // Quitter
*/
// Post-save success cue: positive chime + brief centred message overlay.
// Shared by the regular save (case 73) and debug bug-save (case 2001) paths
// in MainGameMenu. Up to ~1 second on screen — skippable by any fire / action
// / menu input. After the wait, drains those inputs (via InitWaitNoInput) so
// the skip-press doesn't immediately re-trigger menu navigation when control
// returns to the surrounding menu loop.
