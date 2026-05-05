#include "CONSOLE.H"
#include "CONSOLE_STATE.H"
#include "../C_EXTERN.H"
#include "../COMMON.H"
#include "../DIRECTORIES.H"
#include "../PLAYACF.H"
#include "../GAMEMENU.H"
#include "../MUSIC.H"
#include "../AMBIANCE.H"
#include <AIL/SAMPLE.H>
#include "../DISKFUNC.H"
#include "../SAVEGAME.H"
#include "../INVENT.H"
#include <SYSTEM/FILES.H>
#include <SYSTEM/STRING.H>
#include <SVGA/SCREEN.H>
#include <SVGA/DIRTYBOX.H>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <SYSTEM/TIMER.H>

/* libconsole cannot include COMMON.H; keep CONSOLE_STATE.H sentinels in sync (compile fails if they drift). */
#if NUM_CUBE_PHANTOM != CONSOLE_NUM_CUBE_PHANTOM_DEFAULT
#error "CONSOLE_STATE.H CONSOLE_NUM_CUBE_PHANTOM_DEFAULT must match COMMON.H NUM_CUBE_PHANTOM"
#endif
#if FLAG_CHAPTER != CONSOLE_LISTVAR_FLAG_CHAPTER
#error "CONSOLE_STATE.H CONSOLE_LISTVAR_FLAG_CHAPTER must match COMMON.H FLAG_CHAPTER"
#endif

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

static void cmd_give(int argc, const char *argv[]) {
    if (argc < 2) {
        Console_Print("Usage: give <inventory_index> (0..%d)", (int)(MAX_INVENTORY - 1));
        return;
    }
    {
        const char *why = console_avail_in_game_scene();
        if (why) {
            Console_Print("%s", why);
            return;
        }
    }
    Console_Close();
    S32 numvar = (S32)strtol(argv[1], NULL, 0);
    if (numvar < 0 || numvar >= (S32)MAX_INVENTORY) {
        Console_Print("Invalid inventory index %d (valid 0..%d)", numvar, (int)(MAX_INVENTORY - 1));
        return;
    }
    {
        const char *tabwhy = console_give_tabinv_not_ready_reason(numvar);
        if (tabwhy) {
            Console_Print("%s", tabwhy);
            return;
        }
    }
    /* Match script behavior: run found-object cinematic with saved timer context. */
    SaveTimer();
    DoFoundObj(numvar);
    RestoreTimer();
    Console_Print("Gave inventory entry %d", numvar);
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
    /* Mode: 0 = bad end (loads menu screen after), 1 = good end (Twinsen+Zoey).
       Default to 0; pass `credits 1` for the win-path variant. */
    S32 mode = 0;
    if (argc >= 2)
        mode = (S32)strtol(argv[1], NULL, 0);
    Console_Close();
    Credits(mode);
    /* Credits leaves the palette black on exit. Existing engine call sites
       (endgame win path → PlayAcf, demo-timeout → fade-to-black + return to
       menu) intentionally rely on that; doing the fade-in inside Credits
       would regress them. From the console we want the screen visible
       immediately on return, so handle it here for mode=0. */
    if (mode == 0) {
        FadeToPal(PtrPalNormal);
        PtrPal = PtrPalNormal;
    }
    Console_Print("Credits done (mode=%d)", (int)mode);
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
/* Registration */
/*──────────────────────────────────────────────────────────────────────────*/

void Console_RegisterAll(void) {
    Console_RegisterCommandEx("cube", cmd_cube, "Go to cube N (e.g. cube 5)", "cube <number>",
                              "Any state; closes console. Cube change applies next frame.");
    Console_RegisterCommandEx("load", cmd_load, "Load save by player name or file name", "load <save_name>",
                              "Resolves save in save/; closes console then loads.");
    Console_RegisterCommandEx("loadbug", cmd_loadbug, "Load bug save (see listbugs)", "loadbug [name]",
                              "Loads bugs/<name>.lba (default: bug). Closes console.");
    Console_RegisterCommandEx("give", cmd_give, "Give inventory item by flag number", "give <inventory_index>",
                              "Requires in-game loaded cube (not during video).");
    Console_RegisterCommandEx("playvideo", cmd_playvideo, "Play ACF video by name", "playvideo <name>",
                              "Closes console; plays video if found.");
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
    Console_RegisterCommandEx("buildinfo", cmd_buildinfo, "Build timestamp and CMake flags", "(no args)",
                              "Same string as logged at startup.");
    Console_RegisterCommandEx("exit", cmd_exit, "Exit the game immediately", "(no args)", "Clean shutdown.");
    Console_RegisterCommandEx("listsaves", cmd_listsaves, "List save games (names)", "(no args)", "Reads save/ directory.");
    Console_RegisterCommandEx("listbugs", cmd_listbugs, "List bug saves in bugs directory", "(no args)", "Reads bugs/ directory.");
    Console_RegisterCommandEx("timer", cmd_timer, "Advance timer by N ms (default 200)", "timer [ms]", "Advances high-res timer.");
    Console_RegisterCommandEx("status", cmd_status, "Print island, cube, chapter, objs, FPS, pos", "(no args)",
                              "Snapshot of current game state.");
    Console_RegisterCommandEx("screenshot", cmd_screenshot, "Save next frame as PNG (no console)", "(no args)",
                              "PNG under shoot/; console closes first.");
    Console_RegisterCommandEx("savebug", cmd_savebug, "Save current game to bugs dir (optional name)", "savebug [name]",
                              "Requires in-game loaded cube (not during video); writes bugs/<name>.lba.");

    Console_RegisterCvar("fps", &DebugFps, CONSOLE_CVAR_BOOL, "Frame rate display");
    Console_RegisterCvar("horizon", (void *)&FlagDrawHorizon, CONSOLE_CVAR_BOOL_U8, "Draw horizon/cubes around");
    Console_RegisterCvar("zv", &DrawZVObjets, CONSOLE_CVAR_BOOL, "Draw ZV boxes");
}
