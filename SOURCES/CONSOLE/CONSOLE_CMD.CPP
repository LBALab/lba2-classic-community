#include "../AUDIO_CONSOLE.H"
#include <SYSTEM/INPUT_FLOW.H>
#include "CONSOLE.H"
#include "CONSOLE_STATE.H"
#include "CONSOLE_GIVE.H"
#include "../C_EXTERN.H"
#include "../COMMON.H"
#include "../CHEATCOD.H"  // TryExecuteCheatByName, the one verb claim the console offers
#include "../ZONE_GEOM.H" // ZonePointInBox (zonelist shares the engine's zone box test)
#include "../DIRECTORIES.H"
#include "../CONFIG_FILE.H"
#include "../DISTRIB.H"
#include "../FOLLOWCAM.H" // cam_* cvars and the camtrace toggle
#include "../PLAYACF.H"
#include <POLYGON/POLYFILT.H>

extern S32 GfxDitherShading;
#include "../CONFIG.H"
#include "../GAMEMENU.H"
#include "../MUSIC.H"
#include "../AMBIANCE.H"
#include <AIL/SAMPLE.H>
#include "../SAVEGAME.H"
#include "../INPUT.H"
#include "../CONTROL.H" // Control_Input* harness timeline, Control_KeyHold
#include "../RECORD.H"  // session recorder: the `rec` verbs
// K_* scancodes for the `key` command's name table.
#include <SYSTEM/KEYBOARD_KEYS.H>
#include "../JOYSTICK.H"
/* HOLO.H lacks include guards and is pulled in transitively by DEFINES.H. */
#include "../INVENT.H"
#include "../OBJECT.H" // SetAnim (teleport stops the hero's carried animation)
#include "../RES_CATALOG.H"
#include "../RES_SWITCH.H"
#include "../PERFTRACE.H"
#include "../PERSO.H" /* WriteConfigValue (persist one setting) */
#include <SYSTEM/CDTRACKS.H>
#include <SYSTEM/CUE.H>
#include <SYSTEM/DISCIMG.H>
#include <SYSTEM/LOG.H>
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
#include "../BOOT_EXIT.H"

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

/* Per-capture-kind numbering state for unique_shoot_path. */
typedef struct {
    unsigned long stamp;
    int seq;
} T_CAPTURE_SEQ;

/* Build a name under shoot/ that no capture has taken.
 *
 * The stamp has one-second resolution, which is finer than anyone can press a key
 * twice and far coarser than a driver on the control socket can ask: two captures
 * in the same second resolved to one name, and the second quietly replaced the
 * first while reporting success and a path that existed either way. Repeats within
 * a second are numbered from a counter rather than by probing the filesystem,
 * because a screenshot is written during a later present: the first file does not
 * exist yet when the second is named. The probe is the backstop for a second
 * process capturing into the same folder in the same second. */
static void unique_shoot_path(const char *prefix, const char *ext, T_CAPTURE_SEQ *st,
                              char *path, int path_max) {
    char filename[64];
    unsigned long stamp = (unsigned long)time(NULL);
    int spare;

    if (stamp == st->stamp) {
        st->seq++;
    } else {
        st->stamp = stamp;
        st->seq = 0;
    }

    for (spare = 0; spare < 100; spare++) {
        if (st->seq + spare == 0)
            snprintf(filename, sizeof(filename), "%s_%lu.%s", prefix, stamp, ext);
        else
            snprintf(filename, sizeof(filename), "%s_%lu_%d.%s", prefix, stamp,
                     st->seq + spare, ext);
        GetShootPath(path, path_max, filename);
        if (!ExistsFileOrDir(path))
            break;
    }
    st->seq += spare;
}

static void cmd_screenshot(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;
    static T_CAPTURE_SEQ seq = {0, 0};
    char path[ADELINE_MAX_PATH];

    unique_shoot_path("shot", "png", &seq, path, ADELINE_MAX_PATH);
    Console_RequestScreenshot(path, 1); /* 1 = PNG via SavePNG in game */
    Console_Print("Screenshot (no console): %s", path);
}

#ifdef ENABLE_POLY_RECORDING
/* polyrec [path]: arm the polygon draw-call recorder for the next frame, the same
   one-shot request Alt+F9 and --polyrec make, reachable mid-session. --polyrec fires
   once at a tick chosen before boot, so comparing two moments meant two runs; from
   here both come from one, with everything else about the process held fixed.
   The buffer is static because AffScene reads SnapshotName on a later frame. */
static void cmd_polyrec(int argc, const char *argv[]) {
    static char path[ADELINE_MAX_PATH];
    static T_CAPTURE_SEQ seq = {0, 0};

    if (argc >= 2 && argv[1][0])
        snprintf(path, sizeof(path), "%s", argv[1]);
    else
        unique_shoot_path("polyrec", "lba2polyrec", &seq, path, ADELINE_MAX_PATH);

    SnapshotName = path;
    SnapshotRequested = 1;
    Console_Print("polyrec: recording next frame to %s", path);
}
#endif

/* Session recording (docs/RECORDING.md).
   rec start [path] [verbose] | rec stop | rec play [path] | rec info [path]

   The path is optional at both ends, which is what makes this a loop somebody can drive
   rather than a pair of commands with filenames to keep track of: `rec start` names the
   session after the time of day, and `rec play` takes the last one recorded. The names
   go through RECORD.CPP rather than being built here, so the in-game button this is
   groundwork for gets the same behaviour without a console. */
static void cmd_rec(int argc, const char *argv[]) {
    if (argc < 2) {
        Record_Report(NULL);
        return;
    }
    if (!strcmp(argv[1], "start")) {
        /* Record_Path rather than the argument: a name with no directory in it is a
           name in the recordings folder, and with no argument at all there is nothing
           else to report. The argument is still what a failure names, because on that
           side there is no resolved path to point at. */
        const char *asked = NULL;
        int verbose = 0;
        int started;
        int i;
        /* A word in any position rather than a flag in a fixed one: the console splits
           a line on spaces and has no option parser, so `rec start verbose` and
           `rec start walk.rec verbose` both have to read. The cost is that a recording
           cannot be called `verbose`, which is a name nobody wants over a word that
           says what it does. */
        for (i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "verbose"))
                verbose = 1;
            else if (asked == NULL)
                asked = argv[i];
        }
        started = Record_Start(asked, verbose);
        Console_Print(started ? "rec: recording to %s" : "rec: cannot record to %s",
                      started ? Record_Path() : (asked ? asked : "the recordings folder"));
        /* Said here rather than left to the one-off line the recorder prints on its
           first tick: that one goes to stderr, and a player who typed this into the
           console is not reading stderr. */
        if (started && verbose)
            Console_Print("rec: verbose telemetry on; every value the digest mixes, "
                          "every tick");
        return;
    }
    if (!strcmp(argv[1], "stop")) {
        Record_Report(NULL);
        Console_Print(Record_Stop() ? "rec: stopped" : "rec: not active");
        return;
    }
    if (!strcmp(argv[1], "play")) {
        const char *asked = (argc >= 3) ? argv[2] : NULL;
        int playing = Record_Play(asked);
        /* Nothing on the no-argument failure: Record_Play has already said which folder
           it found nothing in, which is the more useful half of the answer. */
        if (playing)
            Console_Print("rec: replaying %s", Record_Path());
        else if (asked)
            Console_Print("rec: cannot open %s", asked);
        return;
    }
    if (!strcmp(argv[1], "squeeze") && argc >= 3) {
        Record_Squeeze(argv[2]);
        return;
    }
    if (!strcmp(argv[1], "info")) {
        Record_Report(argc >= 3 ? argv[2] : NULL);
        return;
    }
    Console_Print("rec: start [path] [verbose] | stop | play [path] | info [path]");
}

/* dumpstate [path]: the JSON snapshot --dump-state writes, on demand. The flag
   captures once as a bounded run ends; a session driven from outside wants one per
   probe, to diff two moments against each other rather than two whole runs. */
static void cmd_dumpstate(int argc, const char *argv[]) {
    static T_CAPTURE_SEQ seq = {0, 0};
    char path[ADELINE_MAX_PATH];

    if (argc >= 2 && argv[1][0]) {
        Control_DumpStateTo(argv[1]);
        Console_Print("dumpstate: wrote %s", argv[1]);
        return;
    }
    unique_shoot_path("state", "json", &seq, path, ADELINE_MAX_PATH);
    Control_DumpStateTo(path);
    Console_Print("dumpstate: wrote %s", path);
}

/* Scancode names for `key`. Only the ones a harness actually reaches for: the two
   Menus bindings the main loop tests, the menu Validate/Cancel keys, and the pad
   equivalents. Anything else can be given as a raw number. */
static const struct {
    const char *name;
    U32 scancode;
} kKeyNames[] = {
    {"esc", K_ESC},
    {"menus", K_F10}, /* the default I_MENUS keyboard binding */
    {"enter", K_ENTER},
    {"space", K_SPACE},
    {"action", K_W},
    {"up", K_GRAY_UP},
    {"down", K_GRAY_DOWN},
    {"left", K_GRAY_LEFT},
    {"right", K_GRAY_RIGHT},
    {"pad-start", K_GAMEPAD_START}, /* the default I_MENUS pad binding */
    {"pad-a", K_GAMEPAD_A},
    {"pad-b", K_GAMEPAD_B},
    {"pad-x", K_GAMEPAD_X},
    {"pad-y", K_GAMEPAD_Y},
};

/* Parse a non-negative count, rejecting trailing garbage and anything below `least`. */
static int console_parse_count(const char *s, long *out, long least) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < least)
        return 0;
    *out = v;
    return 1;
}

/* Parse a signed integer, rejecting trailing garbage. A mouse delta is signed, so it
   cannot go through console_parse_count: `mouse -8 0` would be a bad count rather than a
   leftward drag. */
static int console_parse_signed(const char *s, long *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0')
        return 0;
    *out = v;
    return 1;
}

/* Resolve a `key` name or a raw scancode. Returns 0 for unknown, which is also the
   rejected scancode (SDL_SCANCODE_UNKNOWN), so callers can treat 0 as "no key".
   Shared by the `key` command and `ui askchoice --press`. */
static U32 Console_ScancodeFromName(const char *s) {
    if (!s || !s[0])
        return 0;
    char *end = NULL;
    long raw = strtol(s, &end, 0);
    if (end != s && *end == '\0') {
        /* Range-check here rather than leaving it to Control_KeyHold: a rejected arm is
           a silent no-press, and the caller has already been told the key was scheduled. */
        if (raw <= 0 || raw >= CONTROL_KEY_MAX_SCANCODE)
            return 0;
        return (U32)raw;
    }
    for (size_t i = 0; i < sizeof(kKeyNames) / sizeof(kKeyNames[0]); i++)
        if (!strcasecmp(s, kKeyNames[i].name))
            return kKeyNames[i].scancode;
    return 0;
}

/* ui <surface> <path> — open a UI modal in headless-harness mode, capture a
   PNG of a settled frame, then auto-exit.  The first regression surface for
   the widescreen / UI-layout work, designed so each new surface (holomap,
   dialog, menu-options, ...) plugs in as a parallel subcommand using the
   same composite "open / wait / screenshot / close" pattern.

   Optional prefix flag `--black-bg`: clear the scene to black before the UI
   draws over it (cleanroom golden — UI overlay only, no scene bleed). Only
   inventory, dialog and menu-options honour it; surfaces where the scene is
   part of the captured frame (found-object, video, holomap, holoplan,
   pcx-message, slideshow, menu-main, config) ignore it. The flag is cleared
   at the end of the verb so non-harness callers are unaffected. */
static void cmd_ui_dispatch(int argc, const char *argv[]) {
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
           after the first composited frame; the PNG is written from Log
           before PlayAcf's tail, so the capture is unaffected by it.
           PlayAcf leaves the screen black on exit (its contract) -- fine
           for this headless capture verb. Useful for regression-testing
           widescreen centring of cutscenes. */
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
    if (argc >= 3 && !strcmp(argv[1], "display")) {
        /* Display submenu (Phase 2 runtime-resolution UX). Opens
           DoGameMenu(DisplayMenu) directly via Display_MainCapture,
           skipping the Options-menu detour. Mirrors menu-options's
           pre-fill so the modal composites over a clean background;
           on --black-bg this is solid black per the cleanroom
           convention. */
        if (Console_GetCaptureBlackBg()) {
            memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
            memset(Screen, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        }
        Display_MainCapture(argv[2]);
        Console_Print("ui display -> %s", argv[2]);
        return;
    }
    if (argc >= 3 && !strcmp(argv[1], "resolution")) {
        /* Resolution sub-page (the list opened from the Display submenu's
           Resolution row). Built at runtime, so Resolution_MainCapture
           assembles the menu before driving DoGameMenu. Same black-bg
           convention as `display`. */
        if (Console_GetCaptureBlackBg()) {
            memset(Log, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
            memset(Screen, 0, (size_t)ModeDesiredX * (size_t)ModeDesiredY);
        }
        Resolution_MainCapture(argv[2]);
        Console_Print("ui resolution -> %s", argv[2]);
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
    if (argc >= 4 && !strcmp(argv[1], "askchoice")) {
        /* Dialogue choice list, opened the way LM_ASK_CHOICE does (GERELIFE.CPP):
           fill GameListChoice / GameNbChoices, then call GameAskChoice, which asks
           the question through MyDial, runs DoGameMenu over the options, and speaks
           the chosen line. <question> and each <choice> are text ids in the loaded
           dialogue bank.

              ui askchoice [--press <key>] <question-id> <choice-id>...

           The option list is picked automatically (Enter, once DoGameMenu is up), the
           way the other verbs auto-exit their modal, because a selection scheduled from
           out here cannot know when the question line ends. --press names a key held
           from just after the chosen line starts playing: the only moment a Menus or Esc
           press is a fresh one mid-playback, which is the state issue #451 is about.

           Reports MyKey and Input on return, so a caller can see what the modal left
           behind rather than inferring it from what the game does next. */
        int arg = 2;
        U32 press = 0;
        if (argc >= 4 && !strcmp(argv[arg], "--press")) {
            press = Console_ScancodeFromName(argv[arg + 1]);
            if (!press) {
                Console_Print("ui askchoice: unknown key '%s' (run `key` for the list)",
                              argv[arg + 1]);
                return;
            }
            arg += 2;
        }
        if (argc - arg < 2) {
            Console_Print("Usage: ui askchoice [--press <key>] <question-id> <choice-id>...");
            return;
        }
        S32 question = (S32)atoi(argv[arg++]);
        GameNbChoices = 0;
        /* GameListChoice is S16[10] (GLOBAL.CPP), normally at most 7 in a real script. */
        for (; arg < argc && GameNbChoices < 7; arg++)
            GameListChoice[GameNbChoices++] = (S16)atoi(argv[arg]);
        /* Restored below: NumObjDial decides who a later dialogue bubble points at, so
           leaving it on the hero would follow the run around. `ui holoplan` saves and
           restores HoloMode/Island for the same reason. */
        S32 saved_objdial = NumObjDial;
        NumObjDial = NUM_PERSO;
        Console_Print("ui askchoice: question=%d choices=%d press=%u", (int)question,
                      (int)GameNbChoices, (unsigned)press);
        AskChoice_RequestDrive(K_ENTER, press);
        U32 polls0 = Control_KeyPollCount();
        GameAskChoice(question);
        GameNbChoices = 0;
        NumObjDial = (U8)saved_objdial;
        /* linepolls is the chosen line's own wait, which is what a press cuts short; polls
           is the whole modal, most of it the spoken question. Comparing the line alone is
           what lets a caller tell "the press was consumed" from "the press never arrived"
           without depending on the answer outlasting the question in that bank. */
        Console_Print("ui askchoice done: chose=%d polls=%u linepolls=%u MyKey=%d Input=0x%X",
                      (int)GameChoice, (unsigned)(Control_KeyPollCount() - polls0),
                      (unsigned)AskChoice_LastLinePolls(), (int)MyKey, (unsigned)Input);
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
    Console_Print("Usage: ui [--black-bg] inventory <path>");
    Console_Print("       ui holomap <path>");
    Console_Print("       ui holoplan <island> <path>");
    Console_Print("       ui video <name> <path>");
    Console_Print("       ui [--black-bg] dialog <text-id> <path>");
    Console_Print("       ui [--black-bg] menu-options <path>");
    Console_Print("       ui menu-main <path>");
    Console_Print("       ui [--black-bg] display <path>");
    Console_Print("       ui [--black-bg] resolution <path>");
    Console_Print("       ui config <path>");
    Console_Print("       ui slideshow <path>");
    Console_Print("       ui pcx-message <pcx-idx> <text-id> [<path>] [big|full|normal]");
    Console_Print("       ui found-object <numvar> <path>");
    Console_Print("       ui askchoice [--press <key>] <question-id> <choice-id>...");
}

/* Strip optional `--black-bg` prefix, set the capture-mode flag for the
   duration of the verb, and clear it before returning so subsequent UI
   calls in the same session start from a clean default. */
static void cmd_ui(int argc, const char *argv[]) {
    const char *shifted[16];
    int black_bg = 0;
    if (argc >= 2 && !strcmp(argv[1], "--black-bg")) {
        black_bg = 1;
        int n = argc < 16 ? argc : 16;
        shifted[0] = argv[0];
        for (int i = 1; i < n - 1; i++)
            shifted[i] = argv[i + 1];
        argv = shifted;
        argc--;
    }
    Console_SetCaptureBlackBg(black_bg);
    cmd_ui_dispatch(argc, argv);
    Console_SetCaptureBlackBg(0);
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
        Console_Print("Usage: playvideo <name>  (a video name -- see listvideos)");
        return;
    }
    // Validate the name up front (PlayAcf returns 0 for both "not found" and normal
    // completion, so its return can't distinguish them).
    if (GetNumAcf(argv[1]) < 0) {
        Console_Print("playvideo: no video named '%s' (run listvideos for names)", argv[1]);
        return;
    }
    /* Preview: PlayAcf leaves the screen black on return (its contract -- callers
       reveal via their own fade-in). A bare preview has no such caller, so it
       snapshots the pre-video screen and fades it back, like cmd_credits. Refuse if we
       can't snapshot rather than run PlayAcf's Cls and strand the screen on black. */
    if (!Log) {
        Console_Print("playvideo: no framebuffer");
        return;
    }
    const U32 bufSize = (U32)(ModeDesiredX * ModeDesiredY);
    U8 *savedLog = (U8 *)malloc(bufSize);
    if (!savedLog) {
        Console_Print("playvideo: out of memory for screen snapshot");
        return;
    }
    memcpy(savedLog, Log, bufSize);
    Console_Close();
    PlayAcf(argv[1]);
    memcpy(Log, savedLog, bufSize);
    free(savedLog);
    FadeToPal(PtrPal); // PtrPal is unchanged by a bare PlayAcf; fade the screen back in
    Console_Print("Playing video: %s", argv[1]);
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
/* The name/value mapping lives in DISTRIB.H, shared with the boot banner. */

/* Report what the disc-image source is working with. The boot banner is
   deliberately one line, which is not enough to answer the question a rip
   actually raises: my music is silent, why? Everything the answer needs is
   here in one place, ending with the resolution the music path will make for
   each track the disc might carry. */
static void cmd_disc(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    DiscImageInfo info;
    if (!DiscImage_GetInfo(&info)) {
        Console_Print("No disc image mounted.");
        Console_Print("An image in the game folder is picked up automatically;");
        Console_Print("an install with the files already extracted does not need one.");
        return;
    }

    Console_Print("Image:      %s", info.path);
    /* Name the mode, not just the stride. MODE1 and MODE2 are both 2352-byte
       raw images and differ only in where the user data starts, so the stride
       alone cannot tell a reader which one they have. */
    const char *mode = (info.sectorSize != 2352) ? "cooked"
                       : (info.dataOffset == 24) ? "MODE2 raw"
                                                 : "MODE1 raw";
    Console_Print("Layout:     %d-byte sectors, %s (data at %d), image starts at byte %ld",
                  info.sectorSize, mode, info.dataOffset, info.base);
    Console_Print("Asset root: /%s   (%d files)", info.assetRoot, (int)info.fileCount);
    if (info.rawSectors > 0) {
        Console_Print("Sectors:    %u raw, so in-image CD audio can be read", (unsigned)info.rawSectors);
    } else if (info.trailingBytes > 0) {
        /* Cooked, but with content past the filesystem. That is almost always a
           container keeping its audio at a different stride, and saying so is
           the difference between "nothing to recover" and "convert this". */
        Console_Print("Sectors:    cooked image, and %ld MB past the filesystem this reader",
                      info.trailingBytes / (1024 * 1024));
        Console_Print("            cannot address. If that is CD audio, convert to bin/cue.");
    } else {
        Console_Print("Sectors:    cooked image, no CD audio in it");
    }

    /* The cue is the table of contents. Without one, in-image audio cannot be
       addressed at all, however much of it the image holds. */
    char resDir[ADELINE_MAX_PATH];
    GetResPath(resDir, ADELINE_MAX_PATH, ""); /* empty leaf yields the base dir */
    char cuePath[ADELINE_MAX_PATH] = "";
    CueTrack toc[CUE_MAX_TRACKS];
    int n = Cue_ReadTocInDir(resDir, cuePath, sizeof(cuePath), toc, CUE_MAX_TRACKS);
    if (n <= 0) {
        Console_Print("Cue:        none found beside the image");
    } else {
        Console_Print("Cue:        %s (%d tracks)", cuePath, n);
        for (int i = 0; i < n; i++) {
            Console_Print("  track %02d  %-5s  %-20s  start %u", toc[i].number,
                          toc[i].isAudio ? "audio" : "data", toc[i].file,
                          (unsigned)toc[i].startFrame);
        }
    }

    /* Which of the two redbook rules this install gets. Reporting the per-track
       mapping unconditionally would lie about the single-file case: GOG's cue
       numbers its one external file as track 2, but that file is music track 8,
       and the engine reaches it by falling back rather than by number. */
    int audioCount = 0;
    const CueTrack *loneExternal = NULL;
    for (int i = 0; i < n; i++) {
        if (!toc[i].isAudio)
            continue;
        audioCount++;
        if (!toc[i].fileIsBinary)
            loneExternal = &toc[i];
    }

    if (audioCount >= 2) {
        Console_Print("Music:      matched by CD track number");
        for (int t = CDTRACKS_FIRST_AUDIO; t <= CDTRACKS_LAST_AUDIO; t++) {
            const char *name = CdTracks_NameForNumber(t);
            const CueTrack *hit = NULL;
            for (int i = 0; i < n; i++) {
                if (toc[i].number == t && toc[i].isAudio) {
                    hit = &toc[i];
                    break;
                }
            }
            if (hit == NULL) {
                Console_Print("  %-9s track %02d  not in this cue", name, t);
            } else if (hit->fileIsBinary) {
                Console_Print("  %-9s track %02d  in image at sector %u", name, t,
                              (unsigned)hit->startFrame);
            } else {
                Console_Print("  %-9s track %02d  file %s", name, t, hit->file);
            }
        }
    } else if (loneExternal != NULL) {
        Console_Print("Music:      one external audio file, %s", loneExternal->file);
        Console_Print("            any track request that misses everywhere else plays it");
    } else {
        Console_Print("Music:      no audio tracks in the cue; music comes from files");
    }
}

static void cmd_distrib(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Current: %d (%s)", (int)DistribVersion, Distrib_Name(DistribVersion));
        Console_Print("Available:");
        for (size_t i = 0; i < (size_t)DistribNameCount; i++) {
            Console_Print("  %d  %s", (int)DistribNames[i].version, DistribNames[i].name);
        }
        return;
    }

    /* Whether the publisher splash is drawn, which is a separate question from
       which release this is: a tree that declares one may not want its logo,
       and a re-release that declares nothing may want one. Same persistence as
       the release itself, because the splash runs once in the boot slideshow
       and reading the key later would not bring it back. */
    if (!strcasecmp(argv[1], "logo")) {
        if (argc < 3) {
            Console_Print("Splash: %s (this run)", Distrib_ShowSplash() ? "ON" : "OFF");
            Console_Print("Usage: distrib logo <on|off>");
            return;
        }
        S32 show;
        if (!strcasecmp(argv[2], "on") || !strcmp(argv[2], "1")) {
            show = TRUE;
        } else if (!strcasecmp(argv[2], "off") || !strcmp(argv[2], "0")) {
            show = FALSE;
        } else {
            Console_Print("distrib logo: bad arg '%s' (use on|off).", argv[2]);
            return;
        }
        if (!WriteConfigValue("ShowDistribLogo", show)) {
            Console_Print("Failed to write cfg");
            return;
        }
        Console_Print("Splash %s (next launch)", show ? "ON" : "OFF");
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
        for (size_t i = 0; i < (size_t)DistribNameCount; i++) {
            if (DistribNames[i].version == (S32)n) {
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
        for (size_t i = 0; i < (size_t)DistribNameCount; i++) {
            if (!strcasecmp(argv[1], DistribNames[i].name)) {
                newVer = DistribNames[i].version;
                break;
            }
        }
        if (newVer < 0) {
            Console_Print("Unknown distrib: %s", argv[1]);
            return;
        }
    }

    if (!WriteConfigValue("Version", newVer)) {
        Console_Print("Failed to write cfg");
        return;
    }
    Console_Print("Version %d (%s) (next launch)",
                  (int)newVer, Distrib_Name(newVer));
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

static const char *loglevel_name(LogSeverity sev) {
    switch (sev) {
    case LOG_DEBUG:
        return "debug";
    case LOG_INFO:
        return "info";
    case LOG_WARN:
        return "warn";
    case LOG_ERROR:
        return "error";
    }
    return "info";
}

static void cmd_loglevel(int argc, const char *argv[]) {
    if (argc >= 2) {
        LogSeverity sev;
        if (!Log_ParseLevel(argv[1], &sev)) {
            Console_Print("Unknown level '%s'. Use: debug | info | warn | error",
                          argv[1]);
            return;
        }
        /* The global level gates every sink (file, terminal, and this console),
           so raising it to debug here reveals debug output everywhere. */
        Log_SetLevel(sev);
    }
    Console_Print("log level: %s", loglevel_name(Log_GetLevel()));
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

/* The mode tables, bounds, aspect/snap helpers, and the filter-to-display
   list builder live in RES_CATALOG.{H,CPP} (shared with the Display submenu).
   What stays here is the console's own presentation + parse + apply glue. */

static void res_print_list(const ResModeEntry *list, int count, U32 curW, U32 curH) {
    for (int i = 0; i < count; i++) {
        U32 a, b;
        Res_Aspect(list[i].w, list[i].h, &a, &b);
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
    if (!Res_ValidateDimensions((U32)w, (U32)h))
        return false;
    *outW = (U32)w;
    *outH = (U32)h;
    return true;
}

/* Shared "fire the orchestrator, then arm the keep/revert dialog" path. Pulls
   the safety gate, captures the previous (W, H), closes the console so the
   modal isn't occluded by it, then schedules the dialog on the next tick. */
static void res_do_switch(U32 newW, U32 newH) {
    const char *why = Res_SwitchAllowedReason();
    if (why) {
        Console_Print("%s", why);
        return;
    }
    /* Revert has to put back what the player had chosen. ModeDesiredX/Y is
       whatever the current scene renders at, which in an interior is smaller
       than the choice. */
    U32 prevW = 0, prevH = 0;
    Res_BaseDimensions(&prevW, &prevH);
    if (!Res_Switch(newW, newH)) {
        Console_Print("resolution: switch to %ux%u failed (see log).", newW, newH);
        return;
    }
    if (prevW != newW || prevH != newH) {
        Res_BeginRevertCountdown(prevW, prevH);
        Console_Close();
    }
    Console_Print("Resolution: %ux%u", newW, newH);

    /* The UI is authored on a 640x480 canvas and centred rather than scaled, so
       below that the menus, inventory and holomap lay out past the screen edge.
       The world still renders correctly, which is what these modes are for, so
       they stay selectable and we say what to expect instead. */
    if (newW < UI_CANVAS_W || newH < UI_CANVAS_H)
        Console_Print("  note: below %ux%u the UI is clipped; world render is unaffected.",
                      (unsigned)UI_CANVAS_W, (unsigned)UI_CANVAS_H);
}

static void cmd_perftrace_print_sink(const char *line) {
    Console_Print("%s", line);
}

static void cmd_perftrace(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("perftrace: active=%d. Use start | stop | reset | status | dump [path] | autodump [path]",
                      Perftrace_IsActive());
        Perftrace_PrintStatus(cmd_perftrace_print_sink);
        return;
    }
    const char *sub = argv[1];
    if (!strcmp(sub, "start")) {
        Perftrace_Reset();
        Perftrace_Start();
        Console_Print("perftrace: capture started.");
        return;
    }
    if (!strcmp(sub, "stop")) {
        Perftrace_Stop();
        Console_Print("perftrace: capture stopped.");
        Perftrace_PrintStatus(cmd_perftrace_print_sink);
        return;
    }
    if (!strcmp(sub, "reset")) {
        Perftrace_Reset();
        Console_Print("perftrace: ring buffer cleared.");
        return;
    }
    if (!strcmp(sub, "status")) {
        Perftrace_PrintStatus(cmd_perftrace_print_sink);
        return;
    }
    if (!strcmp(sub, "dump")) {
        const char *path = (argc >= 3) ? argv[2] : "/tmp/perftrace.txt";
        int rc = Perftrace_Dump(path, 0 /* all available */);
        if (rc == 0)
            Console_Print("perftrace: wrote %s", path);
        else
            Console_Print("perftrace: dump failed (open '%s')", path);
        return;
    }
    if (!strcmp(sub, "autodump")) {
        const char *path = (argc >= 3) ? argv[2] : "/tmp/perftrace.txt";
        Perftrace_SetAutoDumpPath(path);
        Console_Print("perftrace: will dump to '%s' on exit", path);
        return;
    }
    Console_Print("perftrace: unknown subcommand '%s'", sub);
}

static void cmd_resolution(int argc, const char *argv[]) {
    /* Cap intentionally generous so cap-overflow can't silently truncate the
       advanced list as it grows over time. */
    ResModeEntry list[64];

    /* No-args: print the recommended-only list and current resolution. */
    if (argc < 2) {
        const int count = Res_BuildCatalog(list, (int)(sizeof(list) / sizeof(list[0])), false);
        Console_Print("Current: %ux%u", ModeDesiredX, ModeDesiredY);
        Console_Print("Available:");
        res_print_list(list, count, ModeDesiredX, ModeDesiredY);
        Console_Print("Usage: resolution <n> | resolution WxH | resolution native | resolution --all");
        return;
    }

    const char *arg = argv[1];

    /* --all: expand to the advanced list. */
    if (!strcmp(arg, "--all")) {
        const int count = Res_BuildCatalog(list, (int)(sizeof(list) / sizeof(list[0])), true);
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
        U32 nw = Res_SnapNativeW(dispW), nh = Res_SnapNativeH(dispH);
        if (!nw || !nh) {
            Console_Print("resolution native: %ux%u out of supported range (%dx%d - %dx%d).",
                          dispW, dispH, RES_MIN_W, RES_MIN_H,
                          RES_MAX_W, RES_MAX_H);
            return;
        }
        res_do_switch(nw, nh);
        return;
    }

    /* Numeric: pick from the recommended (no --all) list. */
    {
        char *end = NULL;
        long n = strtol(arg, &end, 10);
        if (end && *end == '\0') {
            const int count = Res_BuildCatalog(list, (int)(sizeof(list) / sizeof(list[0])), false);
            if (n < 1 || n > count) {
                Console_Print("resolution: index %ld out of range (1..%d). Run 'resolution' to list.",
                              n, count);
                return;
            }
            const ResModeEntry *e = &list[n - 1];
            res_do_switch(e->w, e->h);
            return;
        }
    }

    /* WxH explicit. */
    {
        U32 w = 0, h = 0;
        if (!res_parse_wxh(arg, &w, &h)) {
            Console_Print("resolution: bad target '%s' (need WxH; W%%8==0; range %dx%d - %dx%d).",
                          arg, RES_MIN_W, RES_MIN_H, RES_MAX_W, RES_MAX_H);
            return;
        }
        res_do_switch(w, h);
    }
}

/* vsync [on|off|toggle]: cap the frame rate to the display refresh. No args
   prints the current state. Mirrors the Display-menu Vsync toggle; persists to
   lba2.cfg (VSync key) immediately, the same way the distrib verb does. */
static void cmd_vsync(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Vsync: %s", DisplayVSync ? "ON" : "OFF");
        Console_Print("Usage: vsync <on|off|toggle>");
        return;
    }

    const char *arg = argv[1];
    if (!strcasecmp(arg, "on") || !strcmp(arg, "1")) {
        DisplayVSync = TRUE;
    } else if (!strcasecmp(arg, "off") || !strcmp(arg, "0")) {
        DisplayVSync = FALSE;
    } else if (!strcasecmp(arg, "toggle")) {
        DisplayVSync ^= 1;
    } else {
        Console_Print("vsync: bad arg '%s' (use on|off|toggle).", arg);
        return;
    }

    ConfigFile_MarkVSyncSetThisRun();
    Window_SetVSync(DisplayVSync);
    /* Applied either way; say so separately from whether it was stored, rather
       than reporting a setting that will not survive the run. */
    Console_Print("Vsync: %s", DisplayVSync ? "ON" : "OFF");
    if (!WriteConfigValue("VSync", DisplayVSync)) {
        Console_Print("(this run only: could not write cfg)");
    }
}

/* fixedtimestep [on|off|toggle|<ms>]: issue #358. Throttle the simulation to advance at
   most once per <ms> of game-time (on = 16 = ~60 Hz) so movement/animation stops speeding
   up or freezing above 60 fps (vsync off, high-refresh). off/0 restores per-frame sim.
   Default is on (16); persists to lba2.cfg (FixedTimestep key). See docs/MOVEMENT_FRAMERATE.md. */
static void cmd_fixedtimestep(int argc, const char *argv[]) {
    /* Range and default come from the declaration rather than being written down
       again here: a verb repeating them drifts the moment the row is retuned, and
       these two already disagreed about whether 0 is a value or a rejection. */
    const T_SETTING *decl = ConfigFile_Setting("FixedTimestep");
    const int lo = (decl != NULL) ? (int)decl->min : 0;
    const int hi = (decl != NULL) ? (int)decl->max : 100;
    const int on = (decl != NULL) ? (int)decl->def : 16;

    if (argc >= 2) {
        const char *arg = argv[1];
        if (!strcasecmp(arg, "on")) {
            FixedTimestep = on;
        } else if (!strcasecmp(arg, "off")) {
            FixedTimestep = 0;
        } else if (!strcasecmp(arg, "toggle")) {
            FixedTimestep = FixedTimestep > 0 ? 0 : on;
        } else {
            char *end = NULL;
            const long ms = strtol(arg, &end, 10);
            if (end == arg || *end != '\0' || ms < (long)lo || ms > (long)hi) {
                Console_Print("fixedtimestep: bad arg '%s' (use on|off|toggle|%d..%d).", arg, lo, hi);
                return;
            }
            FixedTimestep = (S32)ms;
        }
        /* After every accepted path and none of the rejected ones, so a bad
           argument does not claim the run set anything. */
        ConfigFile_MarkFixedTimestepSetThisRun();
        if (!WriteConfigValue("FixedTimestep", FixedTimestep)) {
            Console_Print("(this run only: could not write cfg)");
        }
    }

    if (FixedTimestep > 0) {
        Console_Print("FixedTimestep: ON (%d ms, sim <= ~%d fps)", FixedTimestep, 1000 / FixedTimestep);
    } else {
        Console_Print("FixedTimestep: OFF (per-frame simulation)");
    }
    if (argc < 2) {
        Console_Print("Usage: fixedtimestep <on|off|toggle|%d..%d>", lo, hi);
    }
}

/* teleport <x> <y> <z> [beta] | teleport actor <n>: reposition the hero in the current cube.
   Sets ListObjet[NUM_PERSO].Obj coords directly; the next simulated frame re-runs zone detection and
   collision from the new spot. `teleport actor <n>` jumps onto actor n -- the quick way to reach a
   zone-gated interaction without walking. Within-cube only (cross-cube needs ChangeCube). */
static void cmd_teleport(int argc, const char *argv[]) {
    T_OBJET *hero = &ListObjet[NUM_PERSO];

    if (argc >= 3 && !strcasecmp(argv[1], "actor")) {
        int n = atoi(argv[2]);
        if (n < 0 || n >= (int)NbObjets) {
            Console_Print("teleport: actor %d out of range (0..%d)", n, (int)NbObjets - 1);
            return;
        }
        T_OBJET *t = &ListObjet[n];
        hero->Obj.X = t->Obj.X;
        hero->Obj.Y = t->Obj.Y;
        hero->Obj.Z = t->Obj.Z;
        SetAnim(NUM_PERSO, GEN_ANIM_RIEN); // stop the carried walk anim
        Console_Print("teleport: hero -> actor %d at (%d,%d,%d)", n, (int)hero->Obj.X, (int)hero->Obj.Y,
                      (int)hero->Obj.Z);
        return;
    }

    if (argc >= 4) {
        hero->Obj.X = atoi(argv[1]);
        hero->Obj.Y = atoi(argv[2]);
        hero->Obj.Z = atoi(argv[3]);
        if (argc >= 5)
            hero->Obj.Beta = (S16)atoi(argv[4]);
        SetAnim(NUM_PERSO, GEN_ANIM_RIEN);
        Console_Print("teleport: hero -> (%d,%d,%d) beta=%d", (int)hero->Obj.X, (int)hero->Obj.Y,
                      (int)hero->Obj.Z, (int)hero->Obj.Beta);
        return;
    }

    Console_Print("Usage: teleport <x> <y> <z> [beta] | teleport actor <n>");
}

/* varcube <n> [value] / vargame <n> [value]: read or set a scene (cube) or game Life variable.
   With no value, prints the current one; with a value, sets it. Lets a console/harness session drive
   quest state directly -- e.g. arm the flag an NPC's script waits on to reach an interaction stage. */
static void cmd_varcube(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: varcube <n> [value]  (n = 0..%d)", MAX_VARS_CUBE - 1);
        return;
    }
    S32 n = atoi(argv[1]);
    if (n < 0 || n >= MAX_VARS_CUBE) {
        Console_Print("varcube: index %d out of range (0..%d)", n, MAX_VARS_CUBE - 1);
        return;
    }
    if (argc >= 3)
        ListVarCube[n] = (U8)atoi(argv[2]);
    Console_Print("varcube[%d] = %d", n, (int)ListVarCube[n]);
}

static void cmd_vargame(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: vargame <n> [value]  (n = 0..%d)", MAX_VARS_GAME - 1);
        return;
    }
    S32 n = atoi(argv[1]);
    if (n < 0 || n >= MAX_VARS_GAME) {
        Console_Print("vargame: index %d out of range (0..%d)", n, MAX_VARS_GAME - 1);
        return;
    }
    if (argc >= 3)
        ListVarGame[n] = (S16)atoi(argv[2]);
    /* Name the index when the game names it, from the table `give` already resolves
       against: reading `vargame[30] = 1` tells you nothing on its own. The formatting
       is a pure function with a test on it, because the line is parsed and the name
       has to go after the value -- see the contract on Console_FormatVarGameLine. */
    char line[96];
    Console_FormatVarGameLine(line, sizeof(line), n, (S32)ListVarGame[n],
                              Console_GiveItemName(n));
    Console_Print("%s", line);
}

/* The named game variables that are not inventory items, so are not in the `give`
   table. Unlike that table these are the real COMMON.H constants rather than mirrored
   literals -- this file includes COMMON.H, so there is nothing here that can drift and
   nothing for an #error guard to pin. Every FLAG_* the header names is listed except
   FLAG_CHAPTER, which cmd_flags prints on its own line, and FLAG_DONT_USE, which the
   header itself marks as not a flag. */
typedef struct {
    S32 index;
    const char *name;
} QuestFlag;

static const QuestFlag s_questFlags[] = {
    /* Not inventory: the header calls it a "magouille", a scenario var standing in
       because the diving suit is not an inventory item. */
    {FLAG_SCAPHANDRE, "divingsuit"},
    {FLAG_CELEBRATION, "celebration"},
    {FLAG_DINO_VOYAGE, "dinovoyage"},
    {FLAG_ACF, "acf1"}, /* cutscene-seen bitmasks, not booleans */
    {FLAG_ACF2, "acf2"},
    {FLAG_ACF3, "acf3"},
    {FLAG_ESC, "scenarsignal"}, /* OBJECT.CPP sets it to "previent scenar" */
    {FLAG_VEHICULE_PRIS, "vehicletaken"},
    {FLAG_PLANETE_ESMER, "esmerplanet"},
};

/* flags [all]: dump every named game Life variable with its value.
   The quest state a save carries is spread over ListVarGame[], and the indices that
   matter have names in COMMON.H -- FLAG_DIPLOME, the four FLAG_DMKEY_*, FLAG_CHAPTER.
   Answering "what has this save done" meant reading --dump-state's bare integers
   against the header by hand, or forty `vargame` calls. This is one command.
   Nonzero rows only by default, since that is the set that says what has happened;
   `flags all` includes the zeros, for confirming something is genuinely unset. */
static const char *quest_flag_name(S32 index) {
    for (U32 i = 0; i < sizeof(s_questFlags) / sizeof(s_questFlags[0]); i++) {
        if (s_questFlags[i].index == index)
            return s_questFlags[i].name;
    }
    return NULL;
}

static void cmd_flags(int argc, const char *argv[]) {
    int showAll = (argc >= 2 && strcasecmp(argv[1], "all") == 0);
    int shown = 0;

    Console_Print("chapter = %d", (int)ListVarGame[FLAG_CHAPTER]);

    /* Walk the array rather than the tables, so rows come out in index order and an
       aliased index (FLAG_DISQUE_ROUTE / FLAG_VISIONNEUSE both being 24) prints once. */
    for (S32 index = 0; index < MAX_VARS_GAME; index++) {
        if (index == FLAG_CHAPTER)
            continue;
        const char *name = Console_GiveItemName(index);
        if (name == NULL)
            name = quest_flag_name(index);
        if (name == NULL)
            continue;
        S32 value = (S32)ListVarGame[index];
        if (value == 0 && !showAll)
            continue;
        Console_Print("  [%3d] %-16s %d", (int)index, name, (int)value);
        shown++;
    }

    if (shown == 0)
        Console_Print("  (no named flag is set; `flags all` lists the zeros too)");
}

/* behaviour [n] | comportement [n]: read or set the hero behaviour (0=Normal 1=Sportif 2=Aggressive
   3=Discreet 4=Protopack ...). Routes through SetComportement so the weapon/body side-effects match a
   real in-game switch. Lets a headless test reach the melee (Aggressive), jump (Sportif) and hide
   (Discreet) input paths without a save fixture in that behaviour. */
static void cmd_behaviour(int argc, const char *argv[]) {
    if (argc >= 2) {
        S32 n = atoi(argv[1]);
        if (n < 0 || n >= MAX_COMPORTEMENTS) {
            Console_Print("behaviour: %d out of range (0..%d)", n, MAX_COMPORTEMENTS - 1);
            return;
        }
        SetComportement((U8)n);
    }
    Console_Print("behaviour = %d", (int)Comportement);
}

/* weapon [n]: read or set the hero's active weapon global (FLAG_BALLE_MAGIQUE=1, FLAG_SABRE, FLAG_DART,
   FLAG_SARBACANE, ...). Sets Weapon only; a weapon that is ammo- or inventory-gated may also need its
   ListVarGame flag (use `vargame`). For reaching the various fire paths in a headless test. */
static void cmd_weapon(int argc, const char *argv[]) {
    if (argc >= 2)
        Weapon = (U8)atoi(argv[1]);
    Console_Print("weapon = %d", (int)Weapon);
}

/* lifetrace <objN> | off: trace object N's Life script -- its comportement/track/zone state each
   frame and every condition (LF_*) it evaluates. Reveals whether an NPC's script ever reaches a
   given check and what its gating conditions read. */
static void cmd_lifetrace(int argc, const char *argv[]) {
    if (argc >= 2 && !strcasecmp(argv[1], "off"))
        DbgLifeTraceObj = -1;
    else if (argc >= 2)
        DbgLifeTraceObj = atoi(argv[1]);
    if (DbgLifeTraceObj >= 0)
        Console_Print("lifetrace: object %d", DbgLifeTraceObj);
    else
        Console_Print("lifetrace: off");
}

/* objtrace <objN> | off: log object N's world position, animation and track state every simulated
   frame. `lifetrace` answers what a script decided; this answers where the object actually went,
   which is what turns "its path is offset upwards" into a number that can be diffed against a
   known-good run. */
static void cmd_objtrace(int argc, const char *argv[]) {
    if (argc >= 2 && !strcasecmp(argv[1], "off"))
        DbgObjTraceObj = -1;
    else if (argc >= 2)
        DbgObjTraceObj = atoi(argv[1]);
    if (DbgObjTraceObj >= 0)
        Console_Print("objtrace: object %d", DbgObjTraceObj);
    else
        Console_Print("objtrace: off");
}

/* useitem <n> [frames] [delay] | useitem trace <0|1>: inject a one-frame inventory item-use of
   var-game item n (as MenuInventory would), so an NPC's give-item check (LF_USE_INVENTORY) can be
   driven headlessly. `frames` injects over N frames, `delay` waits N frames first (land past a load
   large-jump on a throttle-skip frame). `trace 1` logs each LF_USE_INVENTORY result. */
static void cmd_useitem(int argc, const char *argv[]) {
    if (argc >= 2 && !strcasecmp(argv[1], "trace")) {
        if (argc >= 3)
            DbgUseItemTrace = atoi(argv[2]) ? 1 : 0;
        Console_Print("useitem trace: %s", DbgUseItemTrace ? "ON" : "OFF");
        return;
    }
    if (argc >= 2) {
        DbgUseItemInject = atoi(argv[1]);
        DbgUseItemInjectN = (argc >= 3) ? atoi(argv[2]) : 1;
        DbgUseItemInjectDelay = (argc >= 4) ? atoi(argv[3]) : 0;
        if (DbgUseItemInjectN < 1)
            DbgUseItemInjectN = 1;
        Console_Print("useitem: inject InventoryAction=%d over %d frame(s), after %d-frame delay",
                      DbgUseItemInject, DbgUseItemInjectN, DbgUseItemInjectDelay);
    } else {
        Console_Print("Usage: useitem <n> [frames] [delay] | useitem trace <0|1>");
    }
}

/* cubetrace <0|1>: log every cube transition (island + from -> to cube) as it happens. Run a
   playthrough (or the demo reel) with it on to trace out the game's cube-to-cube flow map. */
static void cmd_cubetrace(int argc, const char *argv[]) {
    if (argc >= 2)
        DbgCubeTrace = atoi(argv[1]) ? 1 : 0;
    Console_Print("cubetrace: %s", DbgCubeTrace ? "ON" : "OFF");
}

/* camnudge <dBeta> [dAlpha] [frames]: drive the analog camera without a mouse or a gamepad. Feeds
   the same per-frame nudge the right stick and the mouse drag feed, for `frames` frames (default 1,
   i.e. a single-frame stick touch). Positive dBeta orbits the way a right-stick push does. */
static void cmd_camnudge(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: camnudge <dBeta> [dAlpha] [frames]");
        return;
    }
    DbgCamNudgeBeta = atoi(argv[1]);
    DbgCamNudgeAlpha = (argc >= 3) ? atoi(argv[2]) : 0;
    DbgCamNudgeFrames = (argc >= 4) ? atoi(argv[3]) : 1;
    if (DbgCamNudgeFrames < 0)
        DbgCamNudgeFrames = 0;
    Console_Print("camnudge: dBeta %d dAlpha %d over %d frame(s)", DbgCamNudgeBeta, DbgCamNudgeAlpha,
                  DbgCamNudgeFrames);
}

/* Bound on an injected mouse delta, in pixels a poll. */
#define MOUSE_INJECT_MAX 10000

/* mouse <dx> <dy> [polls] [buttons] | mouse off: move the mouse, as a device does.
   Feeds the accumulator ManageMouse drains, so the analog camera, the recorder and the
   menus all see an ordinary mouse. `buttons` is the Click mask (1 left, 2 right, 4
   middle) held for the same span, and defaults to 2, because FlagMouseCameraDrag ships
   on and the orbit only runs while the right button is down. `camnudge` is the shortcut
   past all of this, straight to the nudge; this is the long way round on purpose. */
static void cmd_mouse(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: mouse <dx> <dy> [polls] [buttons] | mouse off");
        Console_Print("Pixels per poll. Buttons: 1 left, 2 right, 4 middle (default 2).");
        Console_Print("Polls are MyGetInput calls, the same clock `key` counts.");
        return;
    }
    if (!strcasecmp(argv[1], "off")) {
        Control_MouseClear();
        Console_Print("mouse: cleared");
        return;
    }
    if (argc < 3) {
        Console_Print("mouse: want a dy as well (run `mouse` for the usage)");
        return;
    }

    long dx = 0, dy = 0, polls = 1, buttons = 2;
    if (!console_parse_signed(argv[1], &dx) || !console_parse_signed(argv[2], &dy)) {
        Console_Print("mouse: bad delta (want two integers)");
        return;
    }
    /* A screen's worth of pixels a poll is already absurd, and the accumulator this
       feeds is an S32 that a held motion adds to on every poll: bound it here rather
       than let a typo wrap it into a camera that orbits backwards. */
    if (dx < -MOUSE_INJECT_MAX || dx > MOUSE_INJECT_MAX || dy < -MOUSE_INJECT_MAX ||
        dy > MOUSE_INJECT_MAX) {
        Console_Print("mouse: delta out of range (want -%d..%d pixels a poll)",
                      MOUSE_INJECT_MAX, MOUSE_INJECT_MAX);
        return;
    }
    if (argc >= 4 && !console_parse_count(argv[3], &polls, 1)) {
        Console_Print("mouse: bad poll count '%s' (want a positive integer)", argv[3]);
        return;
    }
    if (argc >= 5 && !console_parse_count(argv[4], &buttons, 0)) {
        Console_Print("mouse: bad button mask '%s' (want a non-negative integer)", argv[4]);
        return;
    }

    if (!Control_MouseMove((S32)dx, (S32)dy, (U32)buttons, (U32)polls)) {
        Console_Print("mouse: could not schedule");
        return;
    }
    Console_Print("mouse: (%ld, %ld) per poll for %ld poll(s), buttons 0x%lX (now %u)", dx, dy,
                  polls, buttons, (unsigned)Control_KeyPollCount());
}

/* stick <x> <y> [polls] | stick off: hold the gamepad right stick, in SDL axis units
   (-32767..32767, deadzone in JoystickDeadzone). Reports a pad present while it runs,
   because the analog camera asks before it reads the axes. The left stick is not here:
   it reaches the game as scancodes in the key table, which `key` already covers. */
static void cmd_stick(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: stick <x> <y> [polls] | stick off");
        Console_Print("SDL axis units, -32767..32767. Right stick only (the left is keys).");
        return;
    }
    if (!strcasecmp(argv[1], "off")) {
        Control_StickClear();
        Console_Print("stick: centred");
        return;
    }
    if (argc < 3) {
        Console_Print("stick: want a y as well (run `stick` for the usage)");
        return;
    }

    long x = 0, y = 0, polls = 1;
    if (!console_parse_signed(argv[1], &x) || !console_parse_signed(argv[2], &y)) {
        Console_Print("stick: bad axis (want two integers)");
        return;
    }
    if (x < -32767 || x > 32767 || y < -32767 || y > 32767) {
        Console_Print("stick: axis out of range (want -32767..32767)");
        return;
    }
    if (argc >= 4 && !console_parse_count(argv[3], &polls, 1)) {
        Console_Print("stick: bad poll count '%s' (want a positive integer)", argv[3]);
        return;
    }

    if (!Control_StickHold((S16)x, (S16)y, (U32)polls)) {
        Console_Print("stick: could not schedule");
        return;
    }
    Console_Print("stick: (%ld, %ld) for %ld poll(s) (now %u)", x, y, polls,
                  (unsigned)Control_KeyPollCount());
}

/* inputflow [reset]: the per-boundary input counters (SYSTEM/INPUT_FLOW.H).
 *
 * One line per boundary, both sides of each, because the number that matters is
 * the gap between them rather than either column alone. `reset` zeroes them so a
 * socket session can measure one scenario at a time from a running engine
 * instead of paying a boot per measurement. */
static void cmd_inputflow(int argc, const char *argv[]) {
    if (argc >= 2 && !strcmp(argv[1], "reset")) {
        InputFlow_Reset();
        Console_Print("inputflow: reset");
        return;
    }

    Console_Print("b1 sdl->polled: down %u up %u -> rises %u over %u poll(s), gap %d",
                  (unsigned)InputFlow.KeyDownEvents, (unsigned)InputFlow.KeyUpEvents,
                  (unsigned)InputFlow.PolledRises, (unsigned)InputFlow.PollsSampled,
                  (int)InputFlow.KeyDownEvents - (int)InputFlow.PolledRises);
    Console_Print("b2 polled->input: keys %u -> actions %u (masked %u), gap %d",
                  (unsigned)InputFlow.TabKeyRises, (unsigned)InputFlow.InputRises,
                  (unsigned)InputFlow.InputRisesMasked,
                  (int)InputFlow.TabKeyRises - (int)InputFlow.InputRises);
    Console_Print("b3 input->sim: %u frame(s) with an edge, %u skipped, %u of those action edges",
                  (unsigned)InputFlow.FramesWithEdge, (unsigned)InputFlow.FramesWithEdgeSkipped,
                  (unsigned)InputFlow.ActionEdgesSkipped);
}

/* skipmodals <0|1>: abort dialogue modals immediately so a headless run does not hang on a
   give/talk dialog (a modal spins waiting for a keypress while the main tick loop is stalled). */
static void cmd_skipmodals(int argc, const char *argv[]) {
    if (argc >= 2)
        DbgSkipModals = atoi(argv[1]) ? 1 : 0;
    Console_Print("skipmodals: %s", DbgSkipModals ? "ON" : "OFF");
}

/* zonelist [type]: list the current cube's scene zones (ListZone) -- box, type, Num and Info fields.
   Zones are invisible to --dump-state (which only reports NbZones), yet they are what drives a
   walkthrough: type 2 (scenaric) sets the hero's ZoneSce, which his Life script reads via
   LF_ZONE_OBJ to award a pickup/fire an event; type 4 (giver) hands an item on action inside the
   box; type 0 changes cube. Printing the boxes turns "walk to the drawer" into a concrete
   `teleport <x> <y> <z>` the harness can drive. Optional [type] filters to one zone type. */
static const char *ZoneTypeName(S32 t) {
    switch (t) {
    case 0:
        return "cube"; // scene transition (Info0..5 = target cube/pos)
    case 1:
        return "camera";
    case 2:
        return "scenaric"; // sets hero ZoneSce = Num -> Life LF_ZONE_OBJ
    case 3:
        return "grid";
    case 4:
        return "giver"; // gives bonus/item on action inside
    case 5:
        return "message";
    case 6:
        return "ladder";
    case 7:
        return "escalator"; // conveyor: Info2 picks the direction
    case 8:
        return "hit";
    case 9:
        return "rail";
    default:
        return "?";
    }
}
static void cmd_zonelist(int argc, const char *argv[]) {
    if (!ListZone || NbZones <= 0) {
        Console_Print("zonelist: no zones (load a scene first)");
        return;
    }
    int filter = (argc >= 2) ? atoi(argv[1]) : -1;
    T_OBJET *hero = &ListObjet[NUM_PERSO];
    Console_Print("zonelist: %d zone(s) in cube %d; hero @ (%d,%d,%d) ZoneSce=%d", (int)NbZones,
                  (int)NumCube, (int)hero->Obj.X, (int)hero->Obj.Y, (int)hero->Obj.Z,
                  (int)hero->ZoneSce);
    T_ZONE *ptrz = ListZone;
    for (S32 n = 0; n < NbZones; n++, ptrz++) {
        if (filter >= 0 && ptrz->Type != filter)
            continue;
        int inside = ZonePointInBox(hero->Obj.X, hero->Obj.Y, hero->Obj.Z, ptrz->X0, ptrz->Y0,
                                    ptrz->Z0, ptrz->X1, ptrz->Y1, ptrz->Z1);
        /* Info7 is the flag word that decides whether a zone does anything, and for a camera
           zone it decides how often: ZONE_ON is the enable, ZONE_OBLIGATOIRE re-aims the camera
           every frame the hero is inside, and without it the zone fires once and latches
           ZONE_ACTIVE until he leaves. Printing the raw byte as well as the names, since a zone
           may carry bits this does not name. */
        char flags[64];
        snprintf(flags, sizeof(flags), "%s%s%s%s",
                 (ptrz->Info7 & ZONE_INIT_ON) ? "init_on " : "",
                 (ptrz->Info7 & ZONE_ON) ? "on " : "",
                 (ptrz->Info7 & ZONE_ACTIVE) ? "active " : "",
                 (ptrz->Info7 & ZONE_OBLIGATOIRE) ? "forced " : "");

        /* All seven Info slots. Which of them carry meaning depends on the type (docs/ZONES.md
           has the per-type map), and the four beyond Info3 are where the interesting authored
           values live: a camera zone keeps its whole shot there, and Info4..Info6 are its
           BetaCam, GammaCam and VueDistance. Printing only Info0..Info3 hid those entirely, so
           anything comparing what a shot asks for against what the camera actually does had to
           rebuild this command first. */
        Console_Print("  [%2d] %-9s Num=%-4d box=(%d,%d,%d)-(%d,%d,%d) info=%d,%d,%d,%d,%d,%d,%d flags=0x%02x %s%s",
                      (int)n, ZoneTypeName(ptrz->Type), (int)ptrz->Num, (int)ptrz->X0, (int)ptrz->Y0,
                      (int)ptrz->Z0, (int)ptrz->X1, (int)ptrz->Y1, (int)ptrz->Z1, (int)ptrz->Info0,
                      (int)ptrz->Info1, (int)ptrz->Info2, (int)ptrz->Info3, (int)ptrz->Info4,
                      (int)ptrz->Info5, (int)ptrz->Info6, (unsigned)ptrz->Info7,
                      flags, inside ? " <-- HERO" : "");
    }
}

/* Parse a `+`/`,`-separated flag list (e.g. "up+throw") or a raw 0xNNN mask into a bitmask.
   strtok-free so it is safe to call while the `seq` parser holds a strtok context. A "-"/"none"
   token is an empty gap (contributes nothing), so a sequence can release everything for a span. */
static bool parse_input_flags(const char *s, U32 *out) {
    static const struct {
        const char *name;
        U32 bit;
    } flags[] = {
        {"up", I_UP},
        {"down", I_DOWN},
        {"left", I_LEFT},
        {"right", I_RIGHT},
        {"action", I_ACTION_M},
        {"search", I_ACTION_ALWAYS},
        {"throw", I_THROW},
        {"esquive", I_ESQUIVE},
        {"inventory", I_INVENTORY},
        {"holomap", I_HOLOMAP},
    };
    char *end = NULL;
    long v = strtol(s, &end, 0);
    if (end != s && *end == '\0') {
        *out = (U32)v; // raw mask
        return true;
    }
    U32 bits = 0;
    char tok[24];
    size_t ti = 0;
    for (const char *p = s;; p++) {
        if (*p == '+' || *p == ',' || *p == '\0') {
            tok[ti] = '\0';
            if (ti > 0 && strcmp(tok, "-") != 0 && strcasecmp(tok, "none") != 0) {
                U32 b = 0;
                for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
                    if (!strcasecmp(tok, flags[i].name)) {
                        b = flags[i].bit;
                        break;
                    }
                if (!b) {
                    Console_Print("input: unknown flag '%s'", tok);
                    return false;
                }
                bits |= b;
            }
            ti = 0;
            if (*p == '\0')
                break;
        } else if (ti < sizeof(tok) - 1) {
            tok[ti++] = *p;
        }
    }
    *out = bits;
    return true;
}

/* Parse a "start:flags:dur[/...]" timeline spec and schedule it, clearing the timeline first.
   frameBased picks the clock: sim ticks (seq) or rendered frames (fseq). Returns the event count,
   or -1 on a parse error (message printed, timeline left cleared). strtok on "/" is safe here
   because parse_input_flags is strtok-free. */
static int parse_and_schedule_seq(const char *spec, int frameBased) {
    const char *name = frameBased ? "fseq" : "seq";
    Control_InputClear();
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", spec);
    int n = 0;
    for (char *ev = strtok(buf, "/"); ev; ev = strtok(NULL, "/")) {
        char *c1 = strchr(ev, ':');
        char *c2 = c1 ? strchr(c1 + 1, ':') : NULL;
        if (!c1 || !c2) {
            Console_Print("input %s: bad event '%s' (want start:flags:dur)", name, ev);
            Control_InputClear();
            return -1;
        }
        *c1 = '\0';
        *c2 = '\0';
        // strtol (not atol) so a non-numeric or empty start/dur is a reported error, not a silent 0.
        char *e1 = NULL, *e2 = NULL;
        long start = strtol(ev, &e1, 10);
        long dur = strtol(c2 + 1, &e2, 10);
        if (*ev == '\0' || *e1 != '\0' || *(c2 + 1) == '\0' || *e2 != '\0' || start < 0 || dur < 0) {
            Console_Print("input %s: bad start/dur in event (want non-negative integers)", name);
            Control_InputClear();
            return -1;
        }
        U32 bits = 0;
        if (!parse_input_flags(c1 + 1, &bits)) {
            Control_InputClear();
            return -1;
        }
        int ok = frameBased ? Control_InputScheduleFrame((U32)start, (U32)dur, bits)
                            : Control_InputSchedule((U32)start, (U32)dur, bits);
        if (!ok) {
            Console_Print("input %s: too many events (max 16)", name);
            Control_InputClear();
            return -1;
        }
        n++;
    }
    return n;
}

/* input <flags> [ticks] | input seq <t:flags:dur[/...]> | input fseq <f:flags:dur[/...]>
   | input trace <0|1> | input off
   Drive the hero headlessly. seq durations are SIM ticks (docs/plan/INPUT_SIM_PLAN.md) so a hold means
   the same gameplay across --fixed-timestep settings (a throttle-skipped frame consumes no tick).
   fseq durations are RENDERED frames, so a press can be placed on a throttle-skipped frame to
   reproduce the input-edge drop. Flags combine with + or , (or a raw 0xNNN mask). Examples:
     input up 120                    float/walk forward for 120 sim ticks
     input up+throw 30               hold forward and fire together for 30 ticks
     input seq 0:up:60/60:up+throw:30  walk 60 ticks, then walk+fire 30 ticks
     input fseq 3:action:4           press action on rendered frames 3-6 (a skip frame at high refresh)
     input trace 1                   log injected mask + hero anim/pos each sim tick */
static void cmd_input(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: input <flags> [ticks] | input seq <t:flags:dur[/...]>");
        Console_Print("       input fseq <f:flags:dur[/...]> | input trace <0|1> | input off");
        Console_Print("Flags: up down left right action search throw esquive inventory holomap (+ , 0xNNN)");
        Console_Print("seq durations are SIM ticks; fseq durations are RENDERED frames (edge-drop repro).");
        return;
    }
    if (!strcasecmp(argv[1], "off") || !strcmp(argv[1], "0")) {
        Control_InputClear();
        Console_Print("input: cleared");
        return;
    }
    if (!strcasecmp(argv[1], "trace")) {
        int on = (argc >= 3) ? atoi(argv[2]) : 1;
        Control_InputTrace(on);
        Console_Print("input trace: %s", on ? "on" : "off");
        return;
    }
    if (!strcasecmp(argv[1], "seq") || !strcasecmp(argv[1], "fseq")) {
        int frameBased = !strcasecmp(argv[1], "fseq");
        if (argc < 3) {
            Console_Print("Usage: input %s <start:flags:dur[/start:flags:dur...]>", argv[1]);
            return;
        }
        int n = parse_and_schedule_seq(argv[2], frameBased);
        if (n >= 0)
            Console_Print("input %s: %d event(s) (%s)", argv[1], n,
                          frameBased ? "rendered frames" : "sim ticks");
        return;
    }
    // input <flags> [ticks]: single hold.
    U32 bits = 0;
    if (!parse_input_flags(argv[1], &bits))
        return;
    if (bits == 0) {
        Control_InputClear();
        Console_Print("input: no bits set (nothing to inject)");
        return;
    }
    long ticks = (argc >= 3) ? atol(argv[2]) : 1;
    if (ticks < 1)
        ticks = 1;
    Control_InputHold(bits, (U32)ticks);
    Console_Print("input: 0x%X for %ld sim tick(s)", (unsigned)bits, ticks);
}

/* key <name|scancode> [polls] [delay] | key off
   Hold a key down at the keyboard layer for `polls` input polls, starting `delay` polls
   from now. Unlike `input`, this reaches modal loops -- menus, dialogue, the choice list --
   because it applies where every MyGetInput reads rather than where MainLoop writes.

   Durations are input polls (one MyGetInput), not sim ticks: a modal advances no ticks, so
   a tick-based delay inside one never elapses. In MainLoop a poll is a rendered frame.

   The delay is what makes a press land INSIDE a modal that has not opened yet, and it is
   usually necessary rather than optional. A menu latches whatever is already held when it
   opens (InitWaitNoInput), so a key held from before the modal starts is deliberately
   masked; arm it to land a few polls after. Examples:
     key menus 60          hold F10 for 60 polls, starting now
     key pad-start 200 4   press pad Start 4 polls from now, hold it for 200
     key esc 30 8          press Esc 8 polls from now (past a menu's entry latch) */
static void cmd_key(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: key <name|scancode> [polls] [delay] | key off");
        Console_Print("Names: esc menus enter space action up down left right");
        Console_Print("       pad-start pad-a pad-b pad-x pad-y (or a raw scancode)");
        Console_Print("Polls are MyGetInput calls, so this reaches modals; `input` does not.");
        return;
    }
    if (!strcasecmp(argv[1], "off")) {
        Control_KeyClear();
        Console_Print("key: cleared");
        return;
    }

    U32 sc = Console_ScancodeFromName(argv[1]);
    if (!sc) {
        Console_Print("key: unknown key '%s' (run `key` for the list)", argv[1]);
        return;
    }

    /* strtol with the end pointer, not atol: `key menus sixty` would otherwise mean
       zero, clamp to one poll, and report success having pressed nothing. */
    long polls = 1, delay = 0;
    if (argc >= 3 && !console_parse_count(argv[2], &polls, 1)) {
        Console_Print("key: bad poll count '%s' (want a positive integer)", argv[2]);
        return;
    }
    if (argc >= 4 && !console_parse_count(argv[3], &delay, 0)) {
        Console_Print("key: bad delay '%s' (want a non-negative integer)", argv[3]);
        return;
    }

    if (!Control_KeyHold(sc, (U32)delay, (U32)polls)) {
        Console_Print("key: could not schedule (scancode %u out of range, or too many holds)",
                      (unsigned)sc);
        return;
    }
    Console_Print("key: scancode %u for %ld poll(s) after %ld (now %u)", (unsigned)sc, polls,
                  delay, (unsigned)Control_KeyPollCount());
}

/*──────────────────────────────────────────────────────────────────────────*/
/* Registration */
/*──────────────────────────────────────────────────────────────────────────*/

void Console_RegisterAll(void) {
    /* The cheat names are game content, so the console core cannot know them.
       It offers each typed verb here first and dispatches its own table only if
       nothing claims it. */
    Console_SetVerbClaim(TryExecuteCheatByName);

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
    Console_RegisterCommandEx("disc", cmd_disc, "Show the mounted disc image, its cue and its music map",
                              "(no args)",
                              "Says nothing is mounted on an install with the files extracted. Use it "
                              "when a disc's music is silent: it shows which CD track each theme wants "
                              "and whether the cue describes it.");
    Console_RegisterCommandEx("distrib", cmd_distrib, "Set distributor (Version) for next launch",
                              "distrib <activision|ea|virgin|... | 0-5> | distrib logo <on|off>",
                              "Persists to lba2.cfg (Version, ShowDistribLogo). Run with no args to show "
                              "current. 'logo' turns the publisher splash on or off without changing which "
                              "release this is.");
    Console_RegisterCommandEx("credits", cmd_credits, "Play credits sequence", "credits [0|1]",
                              "Closes console. Mode: 0=bad end (default), 1=good end. Press any key to skip.");
    Console_RegisterCommandEx("listvideos", cmd_listvideos, "List available ACF video names", "(no args)",
                              "Scans game ACF list.");
    Console_RegisterCommandEx("playjingle", cmd_playjingle, "Play jingle 1-26", "playjingle <1-26>", "Plays jingle by index.");
    Console_RegisterCommandEx("playmusic", cmd_playmusic, "Play music track by number [loop]", "playmusic <num> [loop 0|1]",
                              "Optional loop (default 1).");
    Audio_RegisterCommands();
#ifdef USE_MVIDEO_SMACKER
    Console_RegisterCommandEx("video", cmd_video, "Video commands (log)", "video log <0|1>", "Smacker backend only.");
#endif
    Console_RegisterCommandEx("buildinfo", cmd_buildinfo, "Engine version, build timestamp, and CMake flags", "(no args)",
                              "Same string as logged at startup.");
    Console_RegisterCommandEx("exit", cmd_exit, "Exit the game immediately", "(no args)", "Clean shutdown.");
    Console_RegisterCommandEx("loglevel", cmd_loglevel,
                              "Show or set the log level", "loglevel [debug|info|warn|error]",
                              "Master level for all sinks (this console, adeline.log, and the "
                              "terminal). 'loglevel debug' reveals debug output everywhere. "
                              "Default info; set at launch with --log-level or LBA2_LOG_LEVEL.");
    Console_RegisterCommandEx("listsaves", cmd_listsaves, "List save games (names)", "(no args)", "Reads save/ directory.");
    Console_RegisterCommandEx("listbugs", cmd_listbugs, "List bug saves in bugs directory", "(no args)", "Reads bugs/ directory.");
    Console_RegisterCommandEx("timer", cmd_timer, "Advance timer by N ms (default 200)", "timer [ms]", "Advances high-res timer.");
    Console_RegisterCommandEx("status", cmd_status, "Print island, cube, chapter, objs, FPS, pos", "(no args)",
                              "Snapshot of current game state.");
    Console_RegisterCommandEx("screenshot", cmd_screenshot, "Save next frame as PNG (no console)", "(no args)",
                              "PNG under shoot/; console closes first.");
#ifdef ENABLE_POLY_RECORDING
    Console_RegisterCommandEx("polyrec", cmd_polyrec, "Record the next frame's polygon draw calls",
                              "polyrec [path]",
                              "ENABLE_POLY_RECORDING builds only. Without a path, writes "
                              "polyrec_<time>.lba2polyrec under shoot/.");
#endif
    Console_RegisterCommandEx("rec", cmd_rec, "Record a session, or replay one",
                              "rec start [path] [verbose] | rec stop | rec play [path] | rec "
                              "info [path]",
                              "Without a path, `start` names the session after the time of day "
                              "under recordings/ and `play` takes the last one recorded. "
                              "`verbose` records every value the state digest mixes, so a "
                              "divergence names the field and not only the tick.");
    Console_RegisterCommandEx("dumpstate", cmd_dumpstate, "Write the engine state as JSON",
                              "dumpstate [path]",
                              "Same snapshot as --dump-state, on demand rather than once at exit. "
                              "Without a path, writes state_<time>.json under shoot/. Diffing two "
                              "dumps from one session says what changed between two moments.");
    Console_RegisterCommandEx("ui", cmd_ui, "Open a UI modal and capture a screenshot",
                              "ui {inventory|holomap|menu-options|menu-main|config|slideshow} <path> | ui holoplan <island> <path> | ui video <name> <path> | ui {dialog|found-object} <id> <path> | ui pcx-message <pcx-idx> <text-id> [<path>] [big|full|normal] | ui askchoice [--press <key>] <question-id> <choice-id>...",
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
                              "--all expands the list with advanced modes. The switch applies to this "
                              "run only; lba2.cfg keeps the resolution the game boots at.");

    Console_RegisterCommandEx("vsync", cmd_vsync,
                              "Toggle vertical sync (no args = show state)",
                              "vsync [on|off|toggle]",
                              "Caps the frame rate to the display refresh. Persists to lba2.cfg (VSync key).");

    Console_RegisterCommandEx("fixedtimestep", cmd_fixedtimestep,
                              "Throttle the sim to ~60 Hz so movement is frame-rate independent",
                              "fixedtimestep [on|off|toggle|MS]",
                              "Runs the simulation at most once per MS ms of game-time (on = 16). Fixes "
                              "movement speeding up or freezing above 60 fps (vsync off, high-refresh). "
                              "Default on; persists to lba2.cfg (FixedTimestep key).");

    Console_RegisterCommandEx("teleport", cmd_teleport, "Reposition the hero in the current cube",
                              "teleport <x> <y> <z> [beta] | teleport actor <n>",
                              "Sets the hero's coords directly; next frame re-runs zones/collision. "
                              "'actor <n>' jumps onto actor n. Within-cube only.");
    Console_RegisterCommandEx("behaviour", cmd_behaviour, "Read/set hero behaviour (comportement)",
                              "behaviour [0..13]", "0=Normal 1=Sportif 2=Aggressive 3=Discreet 4=Protopack. Routes through SetComportement.");
    Console_RegisterCommandEx("weapon", cmd_weapon, "Read/set hero active weapon",
                              "weapon [n]", "Sets the Weapon global (1=magic ball). Ammo/inventory may need vargame.");
    Console_RegisterCommandEx("varcube", cmd_varcube, "Read/set a scene (cube) Life variable",
                              "varcube <n> [value]",
                              "Reads the cube var, or sets it with a value. Drives quest state.");
    Console_RegisterCommandEx("vargame", cmd_vargame, "Read/set a game Life variable",
                              "vargame <n> [value]",
                              "Reads the game var, or sets it with a value. Drives quest/inventory state. "
                              "Names the index when the game names it.");
    Console_RegisterCommandEx("flags", cmd_flags, "Dump the named game flags with their values",
                              "flags [all]",
                              "The quest state a save carries: chapter, plus every ListVarGame index "
                              "that has a name (diploma, the four dark-monk keys, inventory). Nonzero "
                              "only; 'all' includes the zeros.");
    Console_RegisterCommandEx("lifetrace", cmd_lifetrace, "Trace an object's Life script",
                              "lifetrace <objN> | lifetrace off",
                              "Logs the object's comportement/track/zone each frame and every LF_ "
                              "condition it evaluates.");
    Console_RegisterCommandEx("objtrace", cmd_objtrace, "Trace an object's position each frame",
                              "objtrace <objN> | objtrace off",
                              "Logs the object's X/Y/Z, animation and track offset every simulated "
                              "frame. Pairs with lifetrace: that says what the script chose, this "
                              "says where the object ended up.");
    Console_RegisterCommandEx("useitem", cmd_useitem, "Inject an inventory item-use (headless)",
                              "useitem <n> [frames] [delay] | useitem trace <0|1>",
                              "Injects InventoryAction=n (drives LF_USE_INVENTORY without menu input); "
                              "'trace 1' logs each result.");
    Console_RegisterCommandEx("cubetrace", cmd_cubetrace, "Log every cube transition",
                              "cubetrace <0|1>",
                              "Logs island + from -> to cube on each ChangeCube, tracing the game's "
                              "cube-to-cube flow across a playthrough or the demo reel.");
    FollowCam_RegisterCommands();
    Console_RegisterCommandEx("inputflow", cmd_inputflow, "Per-boundary input flow counters",
                              "inputflow | inputflow reset",
                              "Counts the input signal on both sides of each layer it crosses and "
                              "reports the gap: events against polled state, polled state against "
                              "action bits, action edges against frames the simulation stepped.");

    Console_RegisterCommandEx("stick", cmd_stick, "Hold the gamepad right stick",
                              "stick <x> <y> [polls] | stick off",
                              "SDL axis units (-32767..32767) held for N input polls, with a "
                              "pad reported present for as long as it runs, which is what the "
                              "analog camera checks before it reads the axes. Right stick only: "
                              "the left one reaches the game as scancodes, so `key` covers it.");
    Console_RegisterCommandEx("mouse", cmd_mouse, "Move the mouse, as a device does",
                              "mouse <dx> <dy> [polls] [buttons] | mouse off",
                              "Pixels per poll into the accumulator ManageMouse drains, with "
                              "the Click mask held over the same span (1 left, 2 right, 4 "
                              "middle; default 2, which is what the drag orbit wants). Drives "
                              "the analog camera through the mouse rather than past it, so a "
                              "session recording carries the motion. `camnudge` is the shortcut "
                              "straight to the nudge.");
    Console_RegisterCommandEx("camnudge", cmd_camnudge, "Inject a manual camera nudge (stick/mouse stand-in)",
                              "camnudge <dBeta> [dAlpha] [frames]",
                              "Feeds the same per-frame nudge the gamepad right stick and the mouse "
                              "drag feed, so the analog camera can be driven headlessly.");
    Console_RegisterCommandEx("skipmodals", cmd_skipmodals, "Auto-advance dialogue modals (headless)",
                              "skipmodals <0|1>",
                              "Aborts dialogue modals immediately so a headless run does not hang on a "
                              "give/talk dialog; the script continues past it.");
    Console_RegisterCommandEx("input", cmd_input, "Inject hero input (drive movement/actions)",
                              "input <flags> [ticks] | input seq <t:flags:dur/...> | input fseq <f:flags:dur/...> | input trace <0|1> | input off",
                              "Holds input bits (up/down/left/right/action/search/throw/esquive/inventory/"
                              "holomap, combine with +) for N SIM ticks, so the harness can walk/turn/act "
                              "the hero the same way at any --fixed-timestep. seq schedules a sim-tick "
                              "timeline; fseq schedules it in rendered frames (to land an edge on a skip frame).");
    Console_RegisterCommandEx("key", cmd_key, "Hold a key down, including inside a modal",
                              "key <name|scancode> [polls] [delay] | key off",
                              "Holds a raw scancode in TabKeys for N input polls (one per "
                              "MyGetInput), so unlike `input` it reaches menus, dialogue and the "
                              "choice list. Names: esc menus enter space action up down left right "
                              "pad-start pad-a pad-b pad-x pad-y. Use [delay] to land a press "
                              "inside a modal that has not opened yet: a menu masks whatever was "
                              "already held when it opened.");
    Console_RegisterCommandEx("zonelist", cmd_zonelist, "List the cube's scene zones (boxes+type)",
                              "zonelist [type]",
                              "Prints each ListZone box, Type (cube/camera/scenaric/giver/message/...), "
                              "Num and Info fields, marking the one the hero is in. The pickup/trigger "
                              "boxes a walkthrough needs; invisible to --dump-state. Optional type filter.");

    Console_RegisterCommandEx("perftrace", cmd_perftrace,
                              "Per-frame timing capture (ring buffer, no log spam)",
                              "perftrace [start | stop | reset | status | dump [path] | autodump [path]]",
                              "Ring buffer of the last 256 frames of (frame, scene, present) timing. "
                              "`start` enables capture, `stop` halts it, `dump` writes a summary file "
                              "(default /tmp/perftrace.txt) with iso vs exterior breakdown. "
                              "`autodump` arms a dump on process exit (useful for headless --exec runs). "
                              "Cost when active is ~1 us/frame; zero when inactive.");

    Console_RegisterCvar("fps", &DebugFps, CONSOLE_CVAR_BOOL, "Frame rate display");
    Console_RegisterCvar("horizon", (void *)&FlagDrawHorizon, CONSOLE_CVAR_BOOL_U8, "Draw horizon/cubes around");
    Console_RegisterCvar("zv", &DrawZVObjets, CONSOLE_CVAR_BOOL, "Draw ZV boxes");

    /* The cheat code's toggle, reachable without typing the cheat. A fixture that
       stands next to something hostile is otherwise racing the hero's health, and
       a hero taking damage plays the hit animation over whatever the fixture came
       to watch. */
    Console_RegisterCvar("godmode", &GodMode, CONSOLE_CVAR_BOOL, "Hero takes no damage");

    /* Which device the last input came from, which the save menu reads to decide
       between typing a name and synthesising one (ChoosePlayerName, GAMEMENU.CPP).
       Only a real key event sets it, so neither branch is reachable from a driven
       run without saying which one is under test. */
    Console_RegisterCvar("input_keyboard", &LastInputWasKeyboard, CONSOLE_CVAR_BOOL,
                         "Last input came from a physical keyboard");

    /* Camera tuning (see docs/CONTROLLER.md). No clamp on set, so values can be
     * pushed past the official ranges while tuning; cfg load clamps and changes
     * persist on exit. */
    Console_RegisterCvar("cam_stick_x", &GamepadCameraSensX, CONSOLE_CVAR_INT, "Right-stick orbit sensitivity (1-10)");
    Console_RegisterCvar("cam_stick_y", &GamepadCameraSensY, CONSOLE_CVAR_INT, "Right-stick elevation sensitivity (1-10)");
    Console_RegisterCvar("cam_stick_invert", &GamepadCameraInvertY, CONSOLE_CVAR_BOOL, "Invert right-stick elevation");
    Console_RegisterCvar("cam_stick_analog", &GamepadCameraAnalog, CONSOLE_CVAR_BOOL, "Analog right-stick camera in Auto cam");
    Console_RegisterCvar("cam_stick_max_x", &GamepadCamMaxBeta, CONSOLE_CVAR_INT, "Right-stick orbit speed ceiling (units/frame at sens 10)");
    Console_RegisterCvar("cam_stick_max_y", &GamepadCamMaxAlpha, CONSOLE_CVAR_INT, "Right-stick elevation speed ceiling (units/frame at sens 10)");
    Console_RegisterCvar("pad_deadzone", &JoystickDeadzone, CONSOLE_CVAR_INT, "Gamepad stick deadzone (SDL units 0-32767)");
    Console_RegisterCvar("cam_mouse", &FlagMouseCamera, CONSOLE_CVAR_BOOL, "Mouse camera enable");
    Console_RegisterCvar("cam_mouse_drag", &FlagMouseCameraDrag, CONSOLE_CVAR_BOOL, "Mouse camera: 1=right-drag, 0=free orbit");
    Console_RegisterCvar("cam_mouse_x", &MouseCameraSensX, CONSOLE_CVAR_INT, "Mouse orbit sensitivity (1-10)");
    Console_RegisterCvar("cam_mouse_y", &MouseCameraSensY, CONSOLE_CVAR_INT, "Mouse elevation sensitivity (1-10)");
    Console_RegisterCvar("cam_mouse_invert", &MouseCameraInvertY, CONSOLE_CVAR_BOOL, "Invert mouse elevation");
    Console_RegisterCvar("cam_mouse_div", &MouseCameraDivisor, CONSOLE_CVAR_INT, "Mouse pixel-to-angle divisor (1-16, lower = faster)");
    Console_RegisterCvar("cam_smooth", &ManualCameraSmoothing, CONSOLE_CVAR_INT, "Manual orbit lerp divisor (1 = snap, higher = laggier)");
    Console_RegisterCvar("cam_glide", &FollowCamOrbitGlide, CONSOLE_CVAR_INT, "Orbit follow-through: % of orbit speed kept per frame after release (0 = stop dead)");
    FollowCam_RegisterCvars();
    ConfigFile_RegisterCvars();
    Audio_RegisterCvars();
}
