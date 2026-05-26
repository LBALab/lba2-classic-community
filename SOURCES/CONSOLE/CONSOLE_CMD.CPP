#include "CONSOLE.H"
#include "CONSOLE_STATE.H"
#include "CONSOLE_GIVE.H"
#include "../C_EXTERN.H"
#include "../COMMON.H"
#include "../DIRECTORIES.H"
#include "../PLAYACF.H"
#include "../CONFIG.H"
#include "../GAMEMENU.H"
#include "../MUSIC.H"
#include "../AMBIANCE.H"
#include <AIL/SAMPLE.H>
#include "../DISKFUNC.H"
#include "../SAVEGAME.H"
#include "../INPUT.H"
/* HOLO.H lacks include guards and is pulled in transitively by DEFINES.H. */
#include "../INVENT.H"
#include "../RES_SWITCH.H"
#include <SYSTEM/FILES.H>
#include <SYSTEM/STRING.H>
#include <SYSTEM/WINDOW.H>
#include <SVGA/SCREEN.H>
#include <SVGA/DIRTYBOX.H>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <SYSTEM/TIMER.H>
#include <SYSTEM/DEFFILE.H>

/* libconsole cannot include COMMON.H; keep CONSOLE_STATE.H sentinels in sync (compile fails if they drift). */
#if NUM_CUBE_PHANTOM != CONSOLE_NUM_CUBE_PHANTOM_DEFAULT
#error "CONSOLE_STATE.H CONSOLE_NUM_CUBE_PHANTOM_DEFAULT must match COMMON.H NUM_CUBE_PHANTOM"
#endif
#if FLAG_CHAPTER != CONSOLE_LISTVAR_FLAG_CHAPTER
#error "CONSOLE_STATE.H CONSOLE_LISTVAR_FLAG_CHAPTER must match COMMON.H FLAG_CHAPTER"
#endif
#if FLAG_MONEY != CONSOLE_GIVE_IDX_MONEY
#error "CONSOLE_GIVE.H CONSOLE_GIVE_IDX_MONEY must match COMMON.H FLAG_MONEY"
#endif
#if FLAG_CLOVER != CONSOLE_GIVE_IDX_CLOVER
#error "CONSOLE_GIVE.H CONSOLE_GIVE_IDX_CLOVER must match COMMON.H FLAG_CLOVER"
#endif
/* MAX_INVENTORY is ((U8)40) — not a plain preprocessor constant, so it cannot
   be checked with #if. Catch any drift at compile time instead. */
typedef char console_give_max_inventory_check[(MAX_INVENTORY == CONSOLE_GIVE_MAX_INVENTORY) ? 1 : -1];

/* Shared gate for give/savebug: user-facing reason strings (in-scene predicate). */
static const char *console_avail_in_game_scene(void) {
    return Console_AvailInGameScene_FromState(
        FlagPlayAcf ? 1 : 0,
        (const void *)PtrScene,
        NumCube,
        NUM_CUBE_PHANTOM);
}

/** DoFoundObj indexes TabInv[numvar].TabObj3D[IdObj3D] (see T_INV_TABOBJ3D_COUNT). */
static const char *console_give_tabinv_not_ready_reason(S32 numvar) {
    const S16 id = TabInv[numvar].IdObj3D;
    if (id < 0 || id >= (S16)T_INV_TABOBJ3D_COUNT)
        return "Not available: inventory data for this slot is not valid (not in a normal play state).";
    return NULL;
}

/*──────────────────────────────────────────────────────────────────────────*/
/* Command handlers */
/*──────────────────────────────────────────────────────────────────────────*/

static void cmd_cube(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: cube <number>");
        return;
    }
    Console_Close();
    NewCube = (S32)strtol(argv[1], NULL, 0);
    Console_Print("Cube %d (change on next frame)", NewCube);
}

static void cmd_load(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: load <save_name>");
        return;
    }

    /* Reconstruct full name from all arguments (so names with spaces work). */
    char targetName[MAX_SIZE_PLAYER_NAME + 1];
    targetName[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1 && strlen(targetName) + 1 < sizeof(targetName) - 1)
            strcat(targetName, " ");
        if (strlen(targetName) + strlen(argv[i]) < sizeof(targetName) - 1)
            strcat(targetName, argv[i]);
    }

    /* First, try to resolve by player name as printed by listsaves. */
    char pathname[ADELINE_MAX_PATH];
    struct dirent *file;
    DIR *dir;
    S32 handle;
    U8 wbyte;
    S32 wlong;
    char playername[MAX_SIZE_PLAYER_NAME + 1];
    int foundByName = 0;

    GetSavePath(pathname, ADELINE_MAX_PATH, NULL);
    dir = opendir(pathname);
    if (dir) {
        while ((file = readdir(dir)) != NULL) {
            if (ade_strcasestr(file->d_name, ".LBA") == NULL)
                continue;
            GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
            struct stat st;
            if (stat(pathname, &st) != 0 || !S_ISREG(st.st_mode))
                continue;
            if (ade_strcasestr(pathname, AUTOSAVE_FILENAME) != NULL || ade_strcasestr(pathname, CURRENTSAVE_FILENAME) != NULL)
                continue;
            GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
            handle = OpenRead(pathname);
            if (!handle)
                continue;
            Read(handle, &wbyte, 1);
            Read(handle, &wlong, sizeof(wlong));
            S32 i = 0;
            do {
                Read(handle, &wbyte, 1);
                playername[i++] = (char)wbyte;
            } while (wbyte != 0 && i < (S32)(sizeof(playername) - 1));
            playername[i] = '\0';
            Close(handle);
            if (playername[0] && strcasecmp(playername, targetName) == 0) {
                /* Found matching save by player name. */
                GetSavePath(GamePathname, ADELINE_MAX_PATH, file->d_name);
                foundByName = 1;
                break;
            }
        }
        closedir(dir);
    }

    /* If no player-name match, fall back to treating input as filename. */
    if (!foundByName) {
        char filename[ADELINE_MAX_PATH];
        size_t len = strlen(targetName);
        if (len >= sizeof(filename))
            len = sizeof(filename) - 1;
        memcpy(filename, targetName, len);
        filename[len] = '\0';

        /* Append .lba if not already present (case-insensitive). */
        const char *dot = strrchr(filename, '.');
        if (!dot || ade_strcasestr(dot, ".lba") != dot) {
            size_t cur = strlen(filename);
            if (cur + 4 < sizeof(filename)) {
                filename[cur + 0] = '.';
                filename[cur + 1] = 'l';
                filename[cur + 2] = 'b';
                filename[cur + 3] = 'a';
                filename[cur + 4] = '\0';
            }
        }

        GetSavePath(GamePathname, ADELINE_MAX_PATH, filename);
        if (!ExistsFileOrDir(GamePathname)) {
            Console_Print("Save not found: %s", targetName);
            return;
        }
    }

    Console_Close();
    FlagLoadGame = TRUE;
    LoadGameNumCube();
    Console_Print("Loading save: %s", targetName);
}

static void cmd_loadbug(int argc, const char *argv[]) {
    const char *name = (argc >= 2 && argv[1][0]) ? argv[1] : "bug";
    char safeName[MAX_SIZE_PLAYER_NAME + 1];
    size_t i, j;

    for (i = 0, j = 0; name[i] && j < MAX_SIZE_PLAYER_NAME; i++) {
        char c = name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
            safeName[j++] = c;
    }
    safeName[j] = '\0';
    if (j == 0) {
        safeName[0] = 'b';
        safeName[1] = 'u';
        safeName[2] = 'g';
        safeName[3] = '\0';
    }

    char relPath[ADELINE_MAX_PATH];
    snprintf(relPath, sizeof(relPath), "bugs%c%s.lba", '/', safeName);
    GetSavePath(GamePathname, ADELINE_MAX_PATH, relPath);
    if (!ExistsFileOrDir(GamePathname)) {
        Console_Print("Bug save not found: %s", safeName);
        return;
    }

    Console_Close();
    FlagLoadGame = TRUE;
    LoadGameNumCube();
    Console_Print("Loading bug save: %s", safeName);
}

static void cmd_savebug(int argc, const char *argv[]) {
    {
        const char *why = console_avail_in_game_scene();
        if (why) {
            Console_Print("%s", why);
            return;
        }
    }
    const char *name = (argc >= 2 && argv[1][0]) ? argv[1] : "bug";
    /* Sanitize name for filename: keep alphanumeric, dash, underscore. */
    char safeName[MAX_SIZE_PLAYER_NAME + 1];
    size_t i, j;
    for (i = 0, j = 0; name[i] && j < MAX_SIZE_PLAYER_NAME; i++) {
        char c = name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
            safeName[j++] = c;
    }
    safeName[j] = '\0';
    if (j == 0) {
        safeName[0] = 'b';
        safeName[1] = 'u';
        safeName[2] = 'g';
        safeName[3] = '\0';
    }
    char bugsDir[ADELINE_MAX_PATH];
    GetSavePath(bugsDir, ADELINE_MAX_PATH, "bugs");
    AddDirIfNot(bugsDir);
    char relPath[ADELINE_MAX_PATH];
    snprintf(relPath, sizeof(relPath), "bugs%c%s.lba", '/', safeName);
    GetSavePath(GamePathname, ADELINE_MAX_PATH, relPath);
    strncpy(PlayerName, safeName, MAX_SIZE_PLAYER_NAME);
    PlayerName[MAX_SIZE_PLAYER_NAME] = '\0';
    Console_Close();
    NumVersion = NUM_VERSION | SAVE_COMPRESS;
    SaveGame(TRUE);
    Console_Print("Bug save: %s", GamePathname);
}

static void cmd_timer(int argc, const char *argv[]) {
    S32 delta = (argc >= 2 && argv[1][0]) ? (S32)strtol(argv[1], NULL, 0) : 200;
    SetTimerHR(TimerRefHR + (U32)delta);
    Console_Print("Timer +%d (now %u)", delta, (unsigned int)TimerRefHR);
}

static void cmd_status(int argc, const char *argv[]) {
    char linebuf[128];
    (void)argc;
    (void)argv;
    Console_FormatStatusIslandLine_FromState(linebuf, sizeof(linebuf), Island, NumCube, ListVarGame, CONSOLE_LISTVAR_FLAG_CHAPTER);
    Console_Print("%s", linebuf);
    Console_Print("Obj: %d  Zones: %d  Tracks: %d", NbObjets, NbZones, NbBrickTrack);
    {
        extern volatile S32 NbFramePerSecond;
        extern volatile U32 TimerRefHR;
        Console_Print("FPS: %d  TimerRefHR: %u", NbFramePerSecond, (unsigned int)TimerRefHR);
    }
    Console_Print("Pos: (%d,%d,%d)", Nxw, Nyw, Nzw);
}

static void cmd_screenshot(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    char filename[64];
    char path[ADELINE_MAX_PATH];
    snprintf(filename, sizeof(filename), "shot_%lu.png", (unsigned long)time(NULL));
    GetShootPath(path, ADELINE_MAX_PATH, filename);
    Console_RequestScreenshot(path, 1); /* 1 = PNG via SavePNG in game */
    Console_Print("Screenshot (no console): %s", path);
}

/* ui <surface> <path> — open a UI modal in headless-harness mode, capture a
   PNG of a settled frame, then auto-exit.  The first regression surface for
   the widescreen / UI-layout work, designed so each new surface (holomap,
   dialog, menu-options, ...) plugs in as a parallel subcommand using the
   same composite "open / wait / screenshot / close" pattern. */
static void cmd_ui(int argc, const char *argv[]) {
    if (argc >= 3 && !strcmp(argv[1], "inventory")) {
        /* MenuInventory's `tempo` argument is virtual game-ms; under fixed-dt
           16 that's ~31 frames of modal loop -- plenty of room past the
           5-frame capture countdown for the wheel to settle into its base
           state.  Without input the wheel just sits on InvSelect (default 0)
           with no per-item zoom (TIME_BEFORE_ZOOM = 1000 ms is past tempo). */
        Inv_RequestCapture(argv[2]);
        MenuInventory(500);
        Console_Print("ui inventory -> %s", argv[2]);
        return;
    }
    if (argc >= 3 && !strcmp(argv[1], "holomap")) {
        /* HoloMap fades to black, opens HoloGlobe (the rotating planet view),
           and runs until FlagHoloEnd.  Holo_RequestCapture arms a one-shot
           SavePNG inside HoloGlobe's loop after the fade-in + zoom-in settle,
           then sets FlagHoloEnd so HoloMap exits cleanly. */
        Holo_RequestCapture(argv[2]);
        HoloMap();
        Console_Print("ui holomap -> %s", argv[2]);
        return;
    }
    if (argc >= 4 && !strcmp(argv[1], "video")) {
        /* PlayAcf opens an ACF / Smacker cinematic by name and runs the
           playback loop until end-of-file, ESC, or I_MENUS input.
           Video_RequestCapture arms a one-shot SavePNG inside the loop
           after the first composited frame, then exits via PlayAcf's
           fin_play cleanup (which restores Log + palette).  Useful for
           regression-testing widescreen centring of cutscenes. */
        Video_RequestCapture(argv[3]);
        PlayAcf(argv[2]);
        Console_Print("ui video %s -> %s", argv[2], argv[3]);
        return;
    }
    if (argc >= 4 && !strcmp(argv[1], "holoplan")) {
        /* Capture the zoomed-in planet/island view for a given island index.
           Drive it through HoloMap so the same pre-render setup the normal
           code path does (PtrMap/ObjPtrMap, dialogue bank, body/anim ptrs,
           clip window, camera save) runs before HoloPlan tries to draw the
           textured globe — calling HoloPlan() directly would segfault inside
           the texture filler.  HoloMode=1 tells HoloMap to dispatch straight
           to HoloPlan(Island) instead of opening HoloGlobe. */
        S32 island = (S32)atoi(argv[2]);
        S32 saved_mode = HoloMode;
        S32 saved_island = Island;
        HoloPlan_RequestCapture(argv[3]);
        HoloMode = 1;
        Island = island;
        HoloMap();
        HoloMode = saved_mode;
        Island = saved_island;
        Console_Print("ui holoplan %d -> %s", (int)island, argv[3]);
        return;
    }
    if (argc >= 4 && !strcmp(argv[1], "dialog")) {
        /* Dial(text, drawscene) opens a dialogue bubble + portrait for the
           given text-id in the currently-loaded dialogue bank.  Dial_Request-
           Capture arms a one-shot SavePNG that fires the first time the
           typewriter completes a page (ret == 0 or 2), then sets flagabort
           so Dial exits cleanly without waiting for input. */
        S32 text_id = (S32)atoi(argv[2]);
        Dial_RequestCapture(argv[3]);
        Dial(text_id, TRUE);
        Console_Print("ui dialog %d -> %s", (int)text_id, argv[3]);
        return;
    }
    if (argc >= 3 && !strcmp(argv[1], "menu-options")) {
        /* OptionsMenu(FALSE) opens the in-game options/pause menu and drives
           DoGameMenu.  Menu_RequestCapture arms a one-shot SavePNG inside
           DoGameMenu's loop; on capture, DoGameMenu returns 1000 (ESC
           sentinel) and OptionsMenu's case-1000 exits cleanly. */
        Menu_RequestCapture(argv[2]);
        OptionsMenu(FALSE);
        Console_Print("ui menu-options -> %s", argv[2]);
        return;
    }
    if (argc >= 3 && !strcmp(argv[1], "menu-main")) {
        /* Boot-time main menu (Resume / New / Load / Options / Quit).
           Menu_MainCapture does the MainGameMenu setup minus the case-driven
           cinematic side effects, arms the capture, and calls DoGameMenu
           directly. */
        Menu_MainCapture(argv[2]);
        Console_Print("ui menu-main -> %s", argv[2]);
        return;
    }
    if (argc >= 3 && !strcmp(argv[1], "slideshow")) {
        /* End-of-island / end-game still-image montage. SlideShow loads
           pre-rendered screenshots from scrshot.hqr, each shown with a story
           caption via Dial. SlideShow_RequestCapture arms a one-shot SavePNG
           inside the first frame's Dial typewriter loop, exits the slideshow
           loop cleanly after the capture fires. Centred PCX + letterboxed
           dialog flow — useful for verifying the Dial_Letterbox path.
           NOTE: requires scrshot.hqr in the game data (typically Demo only);
           returns silently if absent. Use `ui pcx-message` below for a path
           that works on standard retail data. */
        SlideShow_RequestCapture(argv[2]);
        SlideShow();
        SlideShow_RequestCapture(NULL); /* disarm if SlideShow exited some other way */
        Console_Print("ui slideshow -> %s", argv[2]);
        return;
    }
    if (argc >= 4 && !strcmp(argv[1], "pcx-message")) {
        /* EffectPcxMess(pcx, screen, PCX_FADE, mess) — load a screen.hqr PCX
           centred via Image_LoadCentered, then overlay the given dialog text-id
           from the currently-loaded dialog bank. Same Dial_Letterbox path as
           SlideShow / scripted LM_PCX_MESS_OBJ scenes; usable on standard
           retail data (screen.hqr is present everywhere).
           pcx-idx is an absolute PCR_ value (PCR_LOGO=0, PCR_CDROM=60,
           PCR_ACTIVISION=72, ...). EffectPcxMess internally doubles its arg
           to skip interleaved palettes, so we halve here before passing. */
        /* Optional 5th arg `mode` overrides the WinDial selection via the
           harness-only Dial_ForceFlag: "big" → DIAL_BIG (multi-line big
           message box, end-game style), "full" → DIAL_FUL, "normal" →
           DIAL_DEF. Anything else (or omitted) → leave as the text-id's
           encoded mode. */
        S32 pcx_idx = (S32)atoi(argv[2]);
        S32 text_id = (S32)atoi(argv[3]);
        const char *path = (argc >= 5 && argv[4][0] && strcmp(argv[4], "-") != 0)
                               ? argv[4]
                               : NULL;
        const char *mode = (argc >= 6) ? argv[5] : "";
        S32 forced = 0;
        if (!strcmp(mode, "big"))
            forced = DIAL_BIG;
        else if (!strcmp(mode, "full"))
            forced = DIAL_FUL;
        else if (!strcmp(mode, "normal"))
            forced = DIAL_DEF;
        if (path && path[0])
            Dial_RequestCapture(path);
        Dial_ForceFlag = forced;
        EffectPcxMess((U8)(pcx_idx / 2), (U8 *)Screen, PCX_FADE, text_id);
        Dial_ForceFlag = 0;
        if (path && path[0])
            Dial_RequestCapture(NULL);
        Console_Print("ui pcx-message pcx=%d text=%d mode=%s -> %s",
                      (int)pcx_idx, (int)text_id,
                      mode[0] ? mode : "(default)",
                      path ? path : "(no path)");
        return;
    }
    if (argc >= 3 && !strcmp(argv[1], "config")) {
        /* Controls-remapping screen (keyboard). Config_MainCapture loads the
           menu PCX into Screen (the background MenuConfig normally inherits
           from its Main Menu → Options → Controls invocation chain), arms a
           one-shot SavePNG, and calls MenuConfig directly. */
        Config_MainCapture(argv[2]);
        Console_Print("ui config -> %s", argv[2]);
        return;
    }
    if (argc >= 4 && !strcmp(argv[1], "found-object")) {
        /* DoFoundObj(numvar) opens the found-object cinematic: 3D rotation
           of the discovered item alongside a typewriter dialogue.  numvar
           is the TabInv slot index (e.g. FLAG_BALLE_MAGIQUE).
           DoFoundObj_RequestCapture arms a one-shot SavePNG that fires
           after the countdown elapses; the hook sets flag=2 so the modal's
           while-loop exits cleanly. */
        S32 numvar = (S32)atoi(argv[2]);
        DoFoundObj_RequestCapture(argv[3]);
        DoFoundObj(numvar);
        Console_Print("ui found-object %d -> %s", (int)numvar, argv[3]);
        return;
    }
    Console_Print("Usage: ui inventory <path>");
    Console_Print("       ui holomap <path>");
    Console_Print("       ui holoplan <island> <path>");
    Console_Print("       ui video <name> <path>");
    Console_Print("       ui dialog <text-id> <path>");
    Console_Print("       ui menu-options <path>");
    Console_Print("       ui menu-main <path>");
    Console_Print("       ui config <path>");
    Console_Print("       ui slideshow <path>");
    Console_Print("       ui pcx-message <pcx-idx> <text-id> [<path>] [big|full|normal]");
    Console_Print("       ui found-object <numvar> <path>");
}

static void cmd_give(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: give <item> [count]");
        Console_Print("Items:");
        for (S32 i = 0; i < Console_GiveItemRowCount(); i++)
            Console_Print("  %s", Console_GiveItemNameByRow(i));
        return;
    }

    S32 index = -1;
    S32 count = 1;
    {
        const char *err = NULL;
        if (!Console_GiveParse(argc, argv, &index, &count, &err)) {
            Console_Print("%s", err);
            return;
        }
    }
    {
        const char *why = console_avail_in_game_scene();
        if (why) {
            Console_Print("%s", why);
            return;
        }
    }
    Console_Close();

    if (index < (S32)MAX_INVENTORY) {
        const char *tabwhy = console_give_tabinv_not_ready_reason(index);
        if (tabwhy) {
            Console_Print("%s", tabwhy);
            return;
        }
        /* Grant possession, then refresh the inventory grid. For money the
           value lives in the planet-specific gold/zlitos global — set that
           directly, because InitTabIndir() overwrites ListVarGame[FLAG_MONEY]
           from it (INVENT.CPP). */
        if (index == FLAG_MONEY) {
            if (Planet >= 2)
                NbGoldPieces = count;
            else
                NbZlitosPieces = count;
        } else {
            if (count > MAX_S16)
                count = MAX_S16;
            ListVarGame[index] = (S16)count;
        }
        InitTabIndir();
        /* Match script behavior: run found-object cinematic with saved timer context. */
        SaveTimer();
        DoFoundObj(index);
        RestoreTimer();
    } else if (index == FLAG_CLOVER) {
        /* Clover boxes are a separate global with no TabInv slot, so there is
           no found-object cinematic. Grant the boxes and fill them. */
        if (count > MAX_CLOVER_BOX)
            count = MAX_CLOVER_BOX;
        NbCloverBox = (S16)count;
        ListVarGame[FLAG_CLOVER] = (S16)count;
        InitTabIndir();
    }

    if (Console_GiveIsCountItem(index))
        Console_Print("Gave %s x%d", Console_GiveItemName(index), (int)count);
    else
        Console_Print("Gave %s", Console_GiveItemName(index));
}

static void cmd_playvideo(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: playvideo <name>");
        return;
    }
    Console_Close();
    if (PlayAcf(argv[1]) >= 0)
        Console_Print("Playing video: %s", argv[1]);
    else
        Console_Print("Video not found: %s", argv[1]);
}

static void cmd_credits(int argc, const char *argv[]) {
    /* Mode: 0 = "demo wrap" variant (no actors), 1 = "win" variant
       (Twinsen+Zoey). Default to 0. The original engine only invokes Credits
       on win or after the auto-demo, never returning to its prior state, so
       Credits() leaves several global side effects behind (GameInProgress=
       TRUE, main menu screen blitted to Log/Screen, palette black). To make
       console invocation transparent — usable from both the main menu and
       live gameplay without flashing menu graphics or flipping the menu
       into pause-mode — snapshot what Credits will mutate, then restore. */
    S32 mode = 0;
    if (argc >= 2)
        mode = (S32)strtol(argv[1], NULL, 0);
    Console_Close();

    const S32 savedGameInProgress = GameInProgress;
    const U32 bufSize = (U32)(ModeDesiredX * ModeDesiredY);
    U8 *savedLog = (U8 *)malloc(bufSize);
    U8 *savedScreen = (U8 *)malloc(bufSize);
    if (savedLog && Log)
        memcpy(savedLog, Log, bufSize);
    if (savedScreen && Screen)
        memcpy(savedScreen, Screen, bufSize);

    Credits(mode);

    GameInProgress = savedGameInProgress;
    if (savedLog) {
        if (Log)
            memcpy(Log, savedLog, bufSize);
        free(savedLog);
    }
    if (savedScreen) {
        if (Screen)
            memcpy(Screen, savedScreen, bufSize);
        free(savedScreen);
    }

    if (mode == 0) {
        FadeToPal(PtrPalNormal);
        PtrPal = PtrPalNormal;
    }
    /* Push the restored back-buffer to display so the next render (menu draw
       or MainLoop AffScene) starts from the pre-credits frame instead of
       briefly flashing the main menu image Credits left in Log. */
    BoxStaticFullflip();

    Console_Print("Credits done (mode=%d)", (int)mode);
}

/* Console invocation of ShowLogo. Mirrors cmd_credits: ShowLogo writes
   directly into Log + PalettePcx and exits via FadeToBlack — fine in its
   native menu/title flow, disruptive from live gameplay. Snapshot the
   same back-buffers cmd_credits does plus the palette bytes ShowLogo
   overwrites, then restore + flip so the command is transparent from any
   state. The s_slideTable is the extension point for future additions
   (e.g. a community-edition slide); one row per slide. */
static const struct {
    const char *name;
    S32 pcr;
} s_slideTable[] = {
    {"activision", PCR_ACTIVISION},
    {"ea", PCR_EA},
    {"virgin", PCR_VIRGIN},
    {"bumper", PCR_BUMPER},
};

static void cmd_slide(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Available slides:");
        for (size_t i = 0; i < sizeof(s_slideTable) / sizeof(s_slideTable[0]); i++) {
            Console_Print("  %s", s_slideTable[i].name);
        }
        return;
    }

    S32 pcr = -1;
    for (size_t i = 0; i < sizeof(s_slideTable) / sizeof(s_slideTable[0]); i++) {
        if (!strcasecmp(argv[1], s_slideTable[i].name)) {
            pcr = s_slideTable[i].pcr;
            break;
        }
    }
    if (pcr < 0) {
        Console_Print("Unknown slide: %s", argv[1]);
        return;
    }

    Console_Close();

    const U32 bufSize = (U32)(ModeDesiredX * ModeDesiredY);
    U8 *savedLog = (U8 *)malloc(bufSize);
    U8 *savedScreen = (U8 *)malloc(bufSize);
    U8 savedPal[768];
    U8 *savedPtrPal = PtrPal;
    if (savedLog && Log)
        memcpy(savedLog, Log, bufSize);
    if (savedScreen && Screen)
        memcpy(savedScreen, Screen, bufSize);
    memcpy(savedPal, PalettePcx, sizeof(savedPal));

    /* ShowLogo advances TimerRefHR via ManageTime for the slide duration.
       Gameplay AI / animation / movement run off TimerRefHR deltas, so a
       5-second jump on resume wedges movement state machines while
       menu/ESC (no timer dep) still work. SaveTimer/RestoreTimer is the
       standard pattern around modal calls (see cmd_give, MESSAGE.CPP,
       INVENT.CPP). The Init pair swallows any input bits the slide
       loop left set so the next gameplay tick starts clean. */
    SaveTimer();

    ShowLogo(pcr, 5);

    if (savedLog) {
        if (Log)
            memcpy(Log, savedLog, bufSize);
        free(savedLog);
    }
    if (savedScreen) {
        if (Screen)
            memcpy(Screen, savedScreen, bufSize);
        free(savedScreen);
    }
    memcpy(PalettePcx, savedPal, sizeof(savedPal));
    PtrPal = savedPtrPal;
    FadeToPal(PtrPal);
    BoxStaticFullflip();

    InitWaitNoKey();
    InitWaitNoInput(I_FIRE | I_ACTION | I_MENUS);
    RestoreTimer();

    Console_Print("Slide done: %s", argv[1]);
}

/* Set the distributor (cfg `Version` key) for next launch. The cfg key
   name is historic — what's stored is which publisher's edition the
   engine should present (Activision, EA, Virgin, regional variants);
   the user-facing command is named `distrib` to be honest about that.
   Persistence-only: writes to lba2.cfg via DefFileBufferWriteValue.
   Doesn't mutate the in-memory DistribVersion global because several
   derived globals (MessageNoCD, music track table, etc.) are computed
   from it once at startup and won't refresh — partial live application
   would be more confusing than just "takes effect on next launch". */
static const struct {
    const char *name;
    S32 ver;
} s_distribTable[] = {
    {"unknown", UNKNOWN_VERSION},
    {"activision", ACTIVISION_VERSION},
    {"activision_sud", ACTIVISION_SUD_VERSION},
    {"ea", EA_VERSION},
    {"virgin", VIRGIN_VERSION},
    {"virgin_asia", VIRGIN_ASIA_VERSION},
};

static const char *distrib_name_for(S32 ver) {
    for (size_t i = 0; i < sizeof(s_distribTable) / sizeof(s_distribTable[0]); i++) {
        if (s_distribTable[i].ver == ver)
            return s_distribTable[i].name;
    }
    return "?";
}

static void cmd_distrib(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Current: %d (%s)", (int)DistribVersion, distrib_name_for(DistribVersion));
        Console_Print("Available:");
        for (size_t i = 0; i < sizeof(s_distribTable) / sizeof(s_distribTable[0]); i++) {
            Console_Print("  %d  %s", (int)s_distribTable[i].ver, s_distribTable[i].name);
        }
        return;
    }

    /* Accept numeric (0-5) or table-name. Numeric first so "1" wins
       cleanly without colliding on a future name that happens to start
       with a digit. */
    S32 newVer = -1;
    char *end = NULL;
    long n = strtol(argv[1], &end, 0);
    if (end && *end == '\0' && argv[1][0] != '\0') {
        S32 found = FALSE;
        for (size_t i = 0; i < sizeof(s_distribTable) / sizeof(s_distribTable[0]); i++) {
            if (s_distribTable[i].ver == (S32)n) {
                newVer = (S32)n;
                found = TRUE;
                break;
            }
        }
        if (!found) {
            Console_Print("Out of range: %ld (valid 0-5)", n);
            return;
        }
    } else {
        for (size_t i = 0; i < sizeof(s_distribTable) / sizeof(s_distribTable[0]); i++) {
            if (!strcasecmp(argv[1], s_distribTable[i].name)) {
                newVer = s_distribTable[i].ver;
                break;
            }
        }
        if (newVer < 0) {
            Console_Print("Unknown distrib: %s", argv[1]);
            return;
        }
    }

    if (!DefFileBufferWriteValue("Version", newVer)) {
        Console_Print("Failed to write cfg");
        return;
    }
    Console_Print("Version %d (%s) (next launch)",
                  (int)newVer, distrib_name_for(newVer));
}

static void cmd_listvideos(int argc, const char *argv[]) {
    /* GetAcfName has no size limit; use a safe buffer for long names. */
    char buf[128];
    ResetAcfList();
    while (GetAcfName(buf))
        Console_Print("  %s", buf);
}

static void cmd_playjingle(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: playjingle <1-26>");
        return;
    }
    S32 num = (S32)strtol(argv[1], NULL, 0);
    PlayJingle(num);
    Console_Print("Jingle %d", num);
}

static void cmd_listjingles(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
}

static void cmd_playmusic(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: playmusic <num> [loop 0|1]");
        return;
    }
    S32 num = (S32)strtol(argv[1], NULL, 0);
    S32 loop = 1;
    if (argc >= 3 && argv[2][0]) {
        loop = (S32)strtol(argv[2], NULL, 0) ? 1 : 0;
    }
    PlayMusic(num, loop);
    Console_Print("Music %d (loop=%d)", num, loop);
}

static void cmd_listmusic(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
}

static void cmd_playsample(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: playsample <num> [freq] [decal] [repeat] [volume] [pan]");
        Console_Print("  freq: pitchbend (default 0x1000)");
        Console_Print("  decal: random pitch range (default 0)");
        Console_Print("  repeat: 0=loop, >0 repeat count (default 1)");
        Console_Print("  volume: 0-127 (default 127)");
        Console_Print("  pan: 0-127 (default 64)");
        return;
    }
    S32 num = (S32)strtol(argv[1], NULL, 0);
    S32 freq = (argc >= 3 && argv[2][0]) ? (S32)strtol(argv[2], NULL, 0) : 0x1000;
    S32 decal = (argc >= 4 && argv[3][0]) ? (S32)strtol(argv[3], NULL, 0) : 0;
    S32 repeat = (argc >= 5 && argv[4][0]) ? (S32)strtol(argv[4], NULL, 0) : 1;
    S32 volume = (argc >= 6 && argv[5][0]) ? (S32)strtol(argv[5], NULL, 0) : 127;
    S32 pan = (argc >= 7 && argv[6][0]) ? (S32)strtol(argv[6], NULL, 0) : 64;

    HQ_MixSample(num, freq, decal, repeat, pan, volume);
    Console_Print("Sample %d (freq=%d decal=%d repeat=%d vol=%d pan=%d)",
                  num, freq, decal, repeat, volume, pan);
}

static void cmd_listsamples(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
}

static void cmd_audio_help_sample(void) {
    Console_Print("audio sample commands:");
    Console_Print("  audio sample play <id> [freq] [decal] [repeat] [volume] [pan]");
    Console_Print("  audio sample stop_all");
}

static void cmd_audio_help_music(void) {
    Console_Print("audio music commands:");
    Console_Print("  audio music play <track> [loop 0|1]");
    Console_Print("  audio music stop");
}

static void cmd_audio_help_global(void) {
    Console_Print("audio global commands:");
    Console_Print("  audio global pause");
    Console_Print("  audio global resume");
    Console_Print("  audio global stop_all");
    Console_Print("  audio global reset");
    Console_Print("  audio global reverse_stereo <0|1>");
    Console_Print("  audio global log <0|1>");
    Console_Print("  audio global status");
}

static void cmd_audio_help_root(void) {
    Console_Print("audio commands:");
    Console_Print("  audio sample play <id> [freq] [decal] [repeat] [volume] [pan]");
    Console_Print("  audio sample stop_all");
    Console_Print("  audio music  play <track> [loop 0|1]");
    Console_Print("  audio music  stop");
    Console_Print("  audio global pause");
    Console_Print("  audio global resume");
    Console_Print("  audio global stop_all");
    Console_Print("  audio global reset");
    Console_Print("  audio global reverse_stereo <0|1>");
    Console_Print("  audio global log <0|1>");
}

static void cmd_audio(int argc, const char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        cmd_audio_help_root();
        return;
    }

    const char *domain = argv[1];

    /* audio sample ... */
    if (strcmp(domain, "sample") == 0) {
        if (argc < 3 || strcmp(argv[2], "help") == 0) {
            cmd_audio_help_sample();
            return;
        }
        const char *action = argv[2];
        if (strcmp(action, "play") == 0) {
            if (argc < 4) {
                cmd_audio_help_sample();
                return;
            }
            S32 num = (S32)strtol(argv[3], NULL, 0);
            S32 freq = (argc >= 5 && argv[4][0]) ? (S32)strtol(argv[4], NULL, 0) : 0x1000;
            S32 decal = (argc >= 6 && argv[5][0]) ? (S32)strtol(argv[5], NULL, 0) : 0;
            S32 repeat = (argc >= 7 && argv[6][0]) ? (S32)strtol(argv[6], NULL, 0) : 1;
            S32 volume = (argc >= 8 && argv[7][0]) ? (S32)strtol(argv[7], NULL, 0) : 127;
            S32 pan = (argc >= 9 && argv[8][0]) ? (S32)strtol(argv[8], NULL, 0) : 64;
            HQ_MixSample(num, freq, decal, repeat, pan, volume);
            Console_Print("Sample %d (freq=%d decal=%d repeat=%d vol=%d pan=%d)",
                          num, freq, decal, repeat, volume, pan);
            return;
        }
        if (strcmp(action, "stop_all") == 0) {
            HQ_StopSample();
            Console_Print("All samples stopped");
            return;
        }
        cmd_audio_help_sample();
        return;
    }

    /* audio music ... */
    if (strcmp(domain, "music") == 0) {
        if (argc < 3 || strcmp(argv[2], "help") == 0) {
            cmd_audio_help_music();
            return;
        }
        const char *action = argv[2];
        if (strcmp(action, "play") == 0) {
            if (argc < 4) {
                cmd_audio_help_music();
                return;
            }
            S32 num = (S32)strtol(argv[3], NULL, 0);
            S32 loop = 1;
            if (argc >= 5 && argv[4][0]) {
                loop = (S32)strtol(argv[4], NULL, 0) ? 1 : 0;
            }
            PlayMusic(num, loop);
            Console_Print("Music %d (loop=%d)", num, loop);
            return;
        }
        if (strcmp(action, "stop") == 0) {
            StopMusic();
            Console_Print("Music stopped");
            return;
        }
        cmd_audio_help_music();
        return;
    }

    /* audio global ... */
    if (strcmp(domain, "global") == 0) {
        if (argc < 3 || strcmp(argv[2], "help") == 0) {
            cmd_audio_help_global();
            return;
        }
        const char *action = argv[2];
        if (strcmp(action, "pause") == 0) {
            HQ_PauseSamples();
            Console_Print("Audio paused (samples)");
            return;
        }
        if (strcmp(action, "resume") == 0) {
            HQ_ResumeSamples();
            Console_Print("Audio resumed (samples)");
            return;
        }
        if (strcmp(action, "stop_all") == 0) {
            HQ_StopSample();
            Console_Print("All samples stopped");
            return;
        }
        if (strcmp(action, "reset") == 0) {
            HQ_StopSample();
            StopMusic();
            InverseStereoSample(ReverseStereo);
            Console_Print("Audio reset (samples stopped, music stopped, stereo reapplied)");
            return;
        }
        if (strcmp(action, "reverse_stereo") == 0) {
            if (argc < 4) {
                Console_Print("Usage: audio global reverse_stereo <0|1>");
                return;
            }
            S32 flag = (S32)strtol(argv[3], NULL, 0) ? 1 : 0;
            InverseStereoSample(flag);
            Console_Print("Reverse stereo: %s", flag ? "ON" : "OFF");
            return;
        }
        if (strcmp(action, "status") == 0) {
            Console_Print("samplesPaused=%d  Sample_Driver_Enabled=%d",
                          GetSamplesPausedCount(), Sample_Driver_Enabled);
            Console_Print("Audio backend logging: %s",
                          SfxLogIsEnabled() ? "ON" : "OFF");
            return;
        }
        if (strcmp(action, "log") == 0) {
            if (argc < 4) {
                Console_Print("Audio backend logging: %s",
                              SfxLogIsEnabled() ? "ON" : "OFF");
                Console_Print("Usage: audio global log <0|1>");
                return;
            }
            int flag = (int)strtol(argv[3], NULL, 0);
            SfxLogEnable(flag);
            Console_Print("Audio backend logging: %s",
                          SfxLogIsEnabled() ? "ON" : "OFF");
            return;
        }
        cmd_audio_help_global();
        return;
    }

    cmd_audio_help_root();
}

static void cmd_buildinfo(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    /* Mirror what TheEndInfo() logs at exit: version banner + build info.
     * Same content, two surfaces (atexit log + in-game console).
     * Console_Print splits on '\n', so the bracketing newlines in
     * Version's log-formatting collapse cleanly here. */
    Console_Print("%s", Version ? Version : "(no version)");
    Console_Print("%s", BuildInfo ? BuildInfo : "(no build info)");
}

#ifdef USE_MVIDEO_SMACKER
static void cmd_video(int argc, const char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        Console_Print("video commands:");
        Console_Print("  video log <0|1>");
        return;
    }
    if (strcmp(argv[1], "log") == 0) {
        if (argc < 3) {
            Console_Print("Video logging: %s", VideoLogIsEnabled() ? "ON" : "OFF");
            Console_Print("Usage: video log <0|1>");
            return;
        }
        int flag = (int)strtol(argv[2], NULL, 0);
        VideoLogEnable(flag);
        Console_Print("Video logging: %s", VideoLogIsEnabled() ? "ON" : "OFF");
        return;
    }
    Console_Print("Unknown video subcommand. Use: video help");
}
#endif

static void cmd_exit(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    /* Immediate clean exit requested from console. */
    TheEnd(PROGRAM_OK, "Exit from console");
}

static void cmd_listsaves(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    char pathname[ADELINE_MAX_PATH];
    struct dirent *file;
    DIR *dir;
    S32 handle;
    U8 wbyte;
    S32 wlong;
    char playername[MAX_SIZE_PLAYER_NAME + 1];
    S32 nb = 0;

    GetSavePath(pathname, ADELINE_MAX_PATH, NULL);
    dir = opendir(pathname);
    if (!dir) {
        Console_Print("Save directory not found.");
        return;
    }
    while ((file = readdir(dir)) != NULL) {
        if (ade_strcasestr(file->d_name, ".LBA") == NULL)
            continue;
        GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
        struct stat st;
        if (stat(pathname, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        if (ade_strcasestr(pathname, AUTOSAVE_FILENAME) != NULL || ade_strcasestr(pathname, CURRENTSAVE_FILENAME) != NULL)
            continue;
        GetSavePath(pathname, ADELINE_MAX_PATH, file->d_name);
        handle = OpenRead(pathname);
        if (!handle)
            continue;
        Read(handle, &wbyte, 1);
        Read(handle, &wlong, sizeof(wlong));
        S32 i = 0;
        do {
            Read(handle, &wbyte, 1);
            playername[i++] = (char)wbyte;
        } while (wbyte != 0 && i < (S32)(sizeof(playername) - 1));
        playername[i] = '\0';
        Close(handle);
        Console_Print("  %s", playername[0] ? playername : file->d_name);
        nb++;
    }
    closedir(dir);
    Console_Print("Saves: %d", nb);
}

static void cmd_listbugs(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    struct dirent *file;
    DIR *dir;
    S32 nb = 0;
    char pathBugs[ADELINE_MAX_PATH];

    GetSavePath(pathBugs, ADELINE_MAX_PATH, "bugs");
    dir = opendir(pathBugs);
    if (!dir) {
        Console_Print("Bug save directory not found: %s", pathBugs);
        return;
    }
    while ((file = readdir(dir)) != NULL) {
        if (ade_strcasestr(file->d_name, ".LBA") != NULL) {
            Console_Print("  %s", file->d_name);
            nb++;
        }
    }
    closedir(dir);
    Console_Print("Bug saves: %d", nb);
}

/*──────────────────────────────────────────────────────────────────────────*/
/* resolution — runtime render-resolution switch (see docs/RUNTIME_RESOLUTION.md) */
/*──────────────────────────────────────────────────────────────────────────*/

/* Same numeric guardrails as --resolution / Res_Switch. Duplicated here so
   the verb's parser doesn't pull in CONTROL.CPP's private constants. */
#define RES_VERB_MIN_W 320
#define RES_VERB_MAX_W 1920
#define RES_VERB_MIN_H 200
#define RES_VERB_MAX_H 1024

typedef struct {
    U32 w, h;
    const char *tag;
} ResEntry;

/* Always-present recommended modes, ordered. The console filters by display
   ≤ at presentation time; entries that don't fit are dropped. 640x480 is
   pinned at index 1 unconditionally. */
static const ResEntry kRecommended[] = {
    {640, 480, "classic"},        /* 4:3 — the authentic-mode anchor */
    {768, 480, "recommended"},    /* 16:10 */
    {1024, 480, "recommended"},   /* ~21:10 */
    {1280, 720, "experimental"},  /* 16:9 — Y>480, holomap may glitch */
    {1920, 1080, "experimental"}, /* 16:9 — same caveat */
};
#define kRecommendedCount ((int)(sizeof(kRecommended) / sizeof(kRecommended[0])))

/* Curated "advanced" list shown behind --all. Reasonable common modes only
   — full multiples-of-8 enumeration would emit hundreds of entries. */
static const ResEntry kAdvanced[] = {
    {320, 200, "advanced"},
    {320, 240, "advanced"},
    {400, 300, "advanced"},
    {512, 384, "advanced"},
    {800, 600, "advanced"},
    {856, 480, "advanced"},
    {960, 540, "advanced"},
    {1024, 600, "advanced"},
    {1024, 768, "advanced"},
    {1152, 648, "advanced"},
    {1280, 800, "advanced"},
    {1280, 960, "advanced"},
    {1280, 1024, "advanced"},
    {1366, 768, "advanced"},
    {1440, 900, "advanced"},
    {1600, 900, "advanced"},
    {1600, 1024, "advanced"},
    {1680, 1024, "advanced"},
};
#define kAdvancedCount ((int)(sizeof(kAdvanced) / sizeof(kAdvanced[0])))

/* GCD-reduce W,H to its aspect (e.g. 1280,720 -> 16,9). */
static void res_aspect(U32 w, U32 h, U32 *outA, U32 *outB) {
    U32 a = w, b = h;
    while (b) {
        U32 t = b;
        b = a % b;
        a = t;
    }
    if (a == 0)
        a = 1;
    *outA = w / a;
    *outB = h / a;
}

static bool res_validate(U32 w, U32 h) {
    return (w >= RES_VERB_MIN_W && w <= RES_VERB_MAX_W && h >= RES_VERB_MIN_H &&
            h <= RES_VERB_MAX_H && (w & 7) == 0);
}

/* Snap a display width down to the nearest multiple of 8 inside the
   supported range, so "native" works on any monitor (e.g. 1366 -> 1360).
   Returns 0 if the result falls below the minimum width. */
static U32 res_snap_native_w(U32 dispW) {
    if (dispW > RES_VERB_MAX_W)
        dispW = RES_VERB_MAX_W;
    dispW &= ~(U32)7;
    if (dispW < RES_VERB_MIN_W)
        return 0;
    return dispW;
}

static U32 res_snap_native_h(U32 dispH) {
    if (dispH > RES_VERB_MAX_H)
        dispH = RES_VERB_MAX_H;
    if (dispH < RES_VERB_MIN_H)
        return 0;
    return dispH;
}

/* Compose the visible list (recommended ± advanced) into `out`, filtering
   to entries ≤ display. Appends a synthetic "native" entry at the end of
   the recommended portion. Returns count. */
static int res_build_list(ResEntry *out, int cap, bool includeAdvanced) {
    U32 dispW = 0, dispH = 0;
    const bool hasDisplay = Window_GetDisplayDimensions(&dispW, &dispH);
    int n = 0;

    /* Recommended */
    for (int i = 0; i < kRecommendedCount && n < cap; i++) {
        if (i == 0) { /* 640x480 always present */
            out[n++] = kRecommended[i];
            continue;
        }
        if (hasDisplay && (kRecommended[i].w > dispW || kRecommended[i].h > dispH))
            continue;
        out[n++] = kRecommended[i];
    }

    /* Native (synthesised from current display). Only if the snapped value
       isn't already in the list. */
    if (hasDisplay && n < cap) {
        U32 nw = res_snap_native_w(dispW), nh = res_snap_native_h(dispH);
        if (nw && nh) {
            bool dup = false;
            for (int i = 0; i < n; i++)
                if (out[i].w == nw && out[i].h == nh) {
                    dup = true;
                    break;
                }
            if (!dup) {
                out[n].w = nw;
                out[n].h = nh;
                out[n].tag = "native";
                n++;
            }
        }
    }

    if (!includeAdvanced)
        return n;

    /* Advanced — dedupe against what's already in the list. */
    for (int i = 0; i < kAdvancedCount && n < cap; i++) {
        if (hasDisplay && (kAdvanced[i].w > dispW || kAdvanced[i].h > dispH))
            continue;
        bool dup = false;
        for (int k = 0; k < n; k++)
            if (out[k].w == kAdvanced[i].w && out[k].h == kAdvanced[i].h) {
                dup = true;
                break;
            }
        if (dup)
            continue;
        out[n++] = kAdvanced[i];
    }

    return n;
}

static void res_print_list(const ResEntry *list, int count, U32 curW, U32 curH) {
    for (int i = 0; i < count; i++) {
        U32 a, b;
        res_aspect(list[i].w, list[i].h, &a, &b);
        const bool isCurrent = (list[i].w == curW && list[i].h == curH);
        Console_Print("  %2d: %4ux%-4u  (%2u:%-2u, %-12s)%s", i + 1, list[i].w, list[i].h,
                      a, b, list[i].tag, isCurrent ? "  <- current" : "");
    }
}

/* Parses "WxH" / "WXH" into a validated (w, h). Returns true on success. */
static bool res_parse_wxh(const char *s, U32 *outW, U32 *outH) {
    const char *xp = strchr(s, 'x');
    if (xp == NULL)
        xp = strchr(s, 'X');
    if (xp == NULL || xp == s || xp[1] == '\0')
        return false;
    char *end_w = NULL, *end_h = NULL;
    long w = strtol(s, &end_w, 10);
    long h = strtol(xp + 1, &end_h, 10);
    if (end_w != xp || end_h == NULL || *end_h != '\0')
        return false;
    if (w <= 0 || h <= 0)
        return false;
    if (!res_validate((U32)w, (U32)h))
        return false;
    *outW = (U32)w;
    *outH = (U32)h;
    return true;
}

static void cmd_resolution(int argc, const char *argv[]) {
    /* Cap intentionally generous so cap-overflow can't silently truncate the
       advanced list as it grows over time. */
    ResEntry list[64];

    /* No-args: print the recommended-only list and current resolution. */
    if (argc < 2) {
        const int count = res_build_list(list, (int)(sizeof(list) / sizeof(list[0])), false);
        Console_Print("Current: %ux%u", ModeDesiredX, ModeDesiredY);
        Console_Print("Available:");
        res_print_list(list, count, ModeDesiredX, ModeDesiredY);
        Console_Print("Usage: resolution <n> | resolution WxH | resolution native | resolution --all");
        return;
    }

    const char *arg = argv[1];

    /* --all: expand to the advanced list. */
    if (!strcmp(arg, "--all")) {
        const int count = res_build_list(list, (int)(sizeof(list) / sizeof(list[0])), true);
        Console_Print("Current: %ux%u", ModeDesiredX, ModeDesiredY);
        Console_Print("Available (advanced):");
        res_print_list(list, count, ModeDesiredX, ModeDesiredY);
        return;
    }

    /* native: snap the current display dims to the nearest valid WxH. */
    if (!strcmp(arg, "native")) {
        U32 dispW = 0, dispH = 0;
        if (!Window_GetDisplayDimensions(&dispW, &dispH)) {
            Console_Print("resolution native: display dimensions unavailable.");
            return;
        }
        U32 nw = res_snap_native_w(dispW), nh = res_snap_native_h(dispH);
        if (!nw || !nh) {
            Console_Print("resolution native: %ux%u out of supported range (%dx%d - %dx%d).",
                          dispW, dispH, RES_VERB_MIN_W, RES_VERB_MIN_H,
                          RES_VERB_MAX_W, RES_VERB_MAX_H);
            return;
        }
        if (!Res_Switch(nw, nh)) {
            Console_Print("resolution native: Res_Switch to %ux%u failed (see log).", nw, nh);
            return;
        }
        Console_Print("Resolution: %ux%u", nw, nh);
        return;
    }

    /* Numeric: pick from the recommended (no --all) list. */
    {
        char *end = NULL;
        long n = strtol(arg, &end, 10);
        if (end && *end == '\0') {
            const int count = res_build_list(list, (int)(sizeof(list) / sizeof(list[0])), false);
            if (n < 1 || n > count) {
                Console_Print("resolution: index %ld out of range (1..%d). Run 'resolution' to list.",
                              n, count);
                return;
            }
            const ResEntry *e = &list[n - 1];
            if (!Res_Switch(e->w, e->h)) {
                Console_Print("resolution: switch to %ux%u failed (see log).", e->w, e->h);
                return;
            }
            Console_Print("Resolution: %ux%u", e->w, e->h);
            return;
        }
    }

    /* WxH explicit. */
    {
        U32 w = 0, h = 0;
        if (!res_parse_wxh(arg, &w, &h)) {
            Console_Print("resolution: bad target '%s' (need WxH; W%%8==0; range %dx%d - %dx%d).",
                          arg, RES_VERB_MIN_W, RES_VERB_MIN_H, RES_VERB_MAX_W, RES_VERB_MAX_H);
            return;
        }
        if (!Res_Switch(w, h)) {
            Console_Print("resolution: switch to %ux%u failed (see log).", w, h);
            return;
        }
        Console_Print("Resolution: %ux%u", w, h);
    }
}

/*──────────────────────────────────────────────────────────────────────────*/
/* Registration */
/*──────────────────────────────────────────────────────────────────────────*/

void Console_RegisterAll(void) {
    Console_RegisterCommandEx("cube", cmd_cube, "Go to cube N (e.g. cube 5)", "cube <number>",
                              "Any state; closes console. Cube change applies next frame.");
    Console_RegisterCommandEx("load", cmd_load, "Load save by player name or file name", "load <save_name>",
                              "Resolves save in save/; closes console then loads.");
    Console_RegisterCommandEx("loadbug", cmd_loadbug, "Load bug save (see listbugs)", "loadbug [name]",
                              "Loads bugs/<name>.lba (default: bug). Closes console.");
    Console_RegisterCommandEx("give", cmd_give, "Give an inventory item by name (run 'give' to list)",
                              "give <item> [count]",
                              "Grants the item and plays the found-object cinematic. Count applies to "
                              "darts/money/gem/penguin/clover. Requires in-game loaded cube (not during video).");
    Console_RegisterCommandEx("playvideo", cmd_playvideo, "Play ACF video by name", "playvideo <name>",
                              "Closes console; plays video if found.");
    Console_RegisterCommandEx("slide", cmd_slide, "Show a distributor or bumper slide",
                              "slide <activision|ea|virgin|bumper>",
                              "Cosmetic; restores the prior screen + palette on exit. Run with no args to list.");
    Console_RegisterCommandEx("distrib", cmd_distrib, "Set distributor (Version) for next launch",
                              "distrib <activision|ea|virgin|... | 0-5>",
                              "Persists to lba2.cfg (Version key). Run with no args to show current.");
    Console_RegisterCommandEx("credits", cmd_credits, "Play credits sequence", "credits [0|1]",
                              "Closes console. Mode: 0=bad end (default), 1=good end. Press any key to skip.");
    Console_RegisterCommandEx("listvideos", cmd_listvideos, "List available ACF video names", "(no args)",
                              "Scans game ACF list.");
    Console_RegisterCommandEx("playjingle", cmd_playjingle, "Play jingle 1-26", "playjingle <1-26>", "Plays jingle by index.");
    Console_RegisterCommandEx("playmusic", cmd_playmusic, "Play music track by number [loop]", "playmusic <num> [loop 0|1]",
                              "Optional loop (default 1).");
    Console_RegisterCommandEx("playsample", cmd_playsample, "Play sample by index (optional params)",
                              "playsample <num> [freq] [decal] [repeat] [volume] [pan]",
                              "Defaults: freq=0x1000, decal=0, repeat=1, vol=127, pan=64.");
    Console_RegisterCommandEx("audio", cmd_audio, "Audio commands (sample/music/global)", "audio ...",
                              "Use: audio (no args) for subcommand list.");
#ifdef USE_MVIDEO_SMACKER
    Console_RegisterCommandEx("video", cmd_video, "Video commands (log)", "video log <0|1>", "Smacker backend only.");
#endif
    Console_RegisterCommandEx("buildinfo", cmd_buildinfo, "Engine version, build timestamp, and CMake flags", "(no args)",
                              "Same string as logged at startup.");
    Console_RegisterCommandEx("exit", cmd_exit, "Exit the game immediately", "(no args)", "Clean shutdown.");
    Console_RegisterCommandEx("listsaves", cmd_listsaves, "List save games (names)", "(no args)", "Reads save/ directory.");
    Console_RegisterCommandEx("listbugs", cmd_listbugs, "List bug saves in bugs directory", "(no args)", "Reads bugs/ directory.");
    Console_RegisterCommandEx("timer", cmd_timer, "Advance timer by N ms (default 200)", "timer [ms]", "Advances high-res timer.");
    Console_RegisterCommandEx("status", cmd_status, "Print island, cube, chapter, objs, FPS, pos", "(no args)",
                              "Snapshot of current game state.");
    Console_RegisterCommandEx("screenshot", cmd_screenshot, "Save next frame as PNG (no console)", "(no args)",
                              "PNG under shoot/; console closes first.");
    Console_RegisterCommandEx("ui", cmd_ui, "Open a UI modal and capture a screenshot",
                              "ui {inventory|holomap|menu-options|menu-main|config|slideshow} <path> | ui holoplan <island> <path> | ui video <name> <path> | ui {dialog|found-object} <id> <path> | ui pcx-message <pcx-idx> <text-id> [<path>] [big|full|normal]",
                              "Composite verb for UI regression fixtures: opens a UI modal "
                              "with a deterministic auto-exit, writes the settled frame to <path> "
                              "via SavePNG, then closes.  Compose with --load / --exec to drive "
                              "headless captures of UI state.");
    Console_RegisterCommandEx("savebug", cmd_savebug, "Save current game to bugs dir (optional name)", "savebug [name]",
                              "Requires in-game loaded cube (not during video); writes bugs/<name>.lba.");
    Console_RegisterCommandEx("resolution", cmd_resolution,
                              "Switch render resolution at runtime (no args = list)",
                              "resolution [<n> | WxH | native | --all]",
                              "No args lists recommended modes; <n> picks by index; WxH is arbitrary "
                              "(W%8==0, range 320x200..1920x1024); native snaps to the current display; "
                              "--all expands the list with advanced modes. See docs/RUNTIME_RESOLUTION.md.");

    Console_RegisterCvar("fps", &DebugFps, CONSOLE_CVAR_BOOL, "Frame rate display");
    Console_RegisterCvar("horizon", (void *)&FlagDrawHorizon, CONSOLE_CVAR_BOOL_U8, "Draw horizon/cubes around");
    Console_RegisterCvar("zv", &DrawZVObjets, CONSOLE_CVAR_BOOL, "Draw ZV boxes");
}
