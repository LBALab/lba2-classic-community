#include "CONTROL.H"

#include "C_EXTERN.H" /* pulls DEFINES.H (T_OBJET) + game globals */

#include <SYSTEM/INPUT_FLOW.H>
#include <SYSTEM/MOUSE.H> /* the harness mouse writes where a motion event does */
#include "JOYSTICK.H"     /* and the harness stick where the pad poll does */
#include "COMMON.H"
#include "PERSO.H"    /* InitGame, MainLoop, TheEnd */
#include "OBJECT.H"   /* ChangeCube */
#include "GAMEMENU.H" /* BeginDemoSlide */
#include "PATCH.H"    /* NbPatches */
#include "DIRECTORIES.H"
#include "DISTRIB.H"     /* Distrib_Name, for the state description */
#include "FOLLOWCAM.H"   /* Auto-camera state in the --dump-state report */
#include "RES_CATALOG.H" /* RES_MIN_W / RES_MAX_W / RES_MIN_H / RES_MAX_H bounds */
#include "RES_SWITCH.H"  /* Res_Switch for --res-switch-test */
#include "SAVEGAME.H"    /* Save_SetAutoDisabled for --no-autosave */
#include "CONSOLE/CONSOLE.H"
#include "CONTROL_SERVER.H" /* --listen: interactive command socket */

#include <3D/PROJREC.H> /* Projrec_BeginCapture / Projrec_EndCapture */
#include <AIL/SAMPLE.H> /* Sample_DriverPlaysSound: whether audio actually came up */
#include <FILEIO/SAVEPNG.H>
#include <SVGA/SCREEN.H>   /* Screen, ModeDesiredX/Y */
#include <SVGA/VIDEO.H>    /* Palette() */
#include <SYSTEM/STRING.H> /* EqualsIgnoringCaseAndEncoding for --language */
#include <SYSTEM/TIMER.H>  /* TimerRefHR, NbFramePerSecond */
#include <SYSTEM/WINDOW.H> /* Window_SetVSync */

#include <SDL3/SDL.h>     /* SDL_SetHint for --headless (must precede SDL_Init) */
#include "RECORD.H"       /* --record / --replay */
#include <SYSTEM/FILES.H> /* ExistsFileOrDir */
#include <SYSTEM/LOG.H>   /* LogSeverity, Log_ParseLevel for --log-level */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BOOT_EXIT.H"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Config (parsed once) --------------------------------------------------- */
#define CONTROL_EXEC_MAX 1024

static int s_active = 0;
static int s_armed = 0;
static int s_have_listen = 0;
static long s_listen_port = 0;
static int s_listen_started = 0;

static const char *s_load_name = NULL; /* --load value (NULL = none) */
static int s_have_exec = 0;
static char s_exec[CONTROL_EXEC_MAX];

/* --exec-at <tick> <cmds>: console commands scheduled for a later tick, repeatable.
   --exec cannot express "after the scene has settled": it fires on the first tick, and a
   `cube` change only applies on the NEXT frame, so `--exec "cube 154; teleport actor 3"`
   teleports the hero in the OLD cube and the pending change then resets him. Silently. */
#define CONTROL_EXEC_AT_MAX 16
typedef struct {
    long tick;
    int fired;
    char cmds[CONTROL_EXEC_MAX];
} T_CONTROL_EXEC_AT;
static T_CONTROL_EXEC_AT s_exec_at[CONTROL_EXEC_AT_MAX];
static int s_exec_at_count = 0;

/* The attract/demo reel: the menu enters it at cube 193 (GAMEMENU.CPP) and the authored
   demo copies run to 221 (docs/SCENES.md). Used only to warn that --demo is a no-op
   elsewhere, so an off-by-one here costs a spurious warning, not behaviour. */
#define DEMO_CUBE_FIRST 193
#define DEMO_CUBE_LAST 221
static int s_have_tick = 0;
static long s_tick = 0;
static int s_have_fixed_dt = 0;
static long s_fixed_dt = 0;
static int s_have_fixed_timestep = 0;
static long s_fixed_timestep = 0;
static int s_have_vsync = 0;
static int s_vsync = 0;
static int s_have_dump = 0;
static char s_dump_path[ADELINE_MAX_PATH];
static int s_have_shot = 0;
static char s_shot_path[ADELINE_MAX_PATH];
static int s_have_polyrec = 0;
static char s_polyrec_path[ADELINE_MAX_PATH];
static int s_have_projrec = 0;
static char s_projrec_path[ADELINE_MAX_PATH];
static int s_have_projrec_hash = 0;
static char s_projrec_hash_path[ADELINE_MAX_PATH];
static int s_exit = 0;
static int s_demo = 0;
static int s_verbose = 0;
static int s_have_log_level = 0;
static LogSeverity s_log_level = LOG_INFO;

/* --resolution WxH: override the engine's framebuffer dimensions at launch.
   Doesn't imply harness mode — a user can pass this alone for interactive play.
   Default (when not given) is the compile-time RESOLUTION_X/RESOLUTION_Y. */
static int s_have_resolution = 0;
static long s_res_x = 0;
static long s_res_y = 0;
/* Resolution bounds (RES_MIN_W / RES_MAX_W / RES_MIN_H / RES_MAX_H, W%8==0) are
   the shared limits in RES_CATALOG.H: same constants Res_ValidateDimensions and
   the console verb use, so the CLI can't drift from what Res_Switch accepts.
   Min = VGA 320x200 (below this the iso projection origin ModeDesiredX/2 - 32 can
   go negative and the 640x480-authored UI clips heavily); max Y = ADELINE_MAX_Y_RES,
   the size of TabOffLine[]/TabOffPhysLine[]. Reachable HD modes: 1280x720, 1920x1080. */

/* --language <name>: override the engine's Language global at boot, bypassing
   the cfg's Language key. Doesn't write anything to disk; the cfg is not touched.
   Index matches MESSAGE.CPP's TabLanguage[] — 0=English, 1=Français, 2=Deutsch,
   3=Español, 4=Italiano, 5=Portugues. Applied in PERSO.CPP after InitLanguage()
   returns. */
static int s_have_language = 0;
static int s_language = 0;
#define CONTROL_LANG_COUNT 6
static const char *kControlLanguageFull[CONTROL_LANG_COUNT] = {
    "English", "Français", "Deutsch", "Español", "Italiano", "Portugues"};
static const char *kControlLanguageShort[CONTROL_LANG_COUNT] = {
    "EN", "FR", "DE", "SP", "IT", "PO"};

/* --no-audio: skip InitAIL() and InitSampleDriver() at engine boot. Pure boolean
   override; the harness uses it to bypass the SDL dummy driver's nanosleep pacing
   that dominates sys time under SDL_AUDIODRIVER=dummy. See INITADEL.C for the
   gate. */
static int s_no_audio = 0;
static int s_headless = 0;
static int s_ignore_focus = 0; /* --ignore-focus */
/* --record / --replay. The `rec` console verbs are the other lever and
   answer a different question: a verb starts where the player chooses, mid-session,
   with no boot or menu in the file. These start at the first tick, so a recording
   covers the menu, the loading path and the scene entry, which is where the fixtures
   are thinnest and where a verb can never reach. */
static const char *s_record_path = NULL;
static const char *s_replay_path = NULL;

/* --res-switch-test WxH@TICK: harness validation for the runtime
   resolution-switch orchestrator (see docs/RUNTIME_RESOLUTION.md and
   SOURCES/RES_SWITCH.CPP). Fires Res_Switch(W, H) exactly once on tick TICK
   from inside Control_TickHook, so the engine has booted and a cube is
   loaded by the time we tear the framebuffers down. Throwaway flag — once
   the console verb / menu path lands, this can go. */
static int s_have_res_switch_test = 0;
static long s_res_switch_test_w = 0;
static long s_res_switch_test_h = 0;
static long s_res_switch_test_tick = 0;
static int s_res_switch_test_fired = 0;

/* --- Runtime ---------------------------------------------------------------- */
static long s_hook_calls = 0;
static int s_finalized = 0;

static void control_dump_state(const char *path);
static void control_screenshot(const char *path);
static void control_console_echo(const char *line);
int Control_UnfiredExecAt(void);

/* --- Parsing ---------------------------------------------------------------- */
static void copy_arg(char *dst, size_t cap, const char *src) {
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

void Control_ParseArgs(int *argc, char *argv[]) {
    int read = 1, write = 1;
    while (read < *argc) {
        const char *a = argv[read];
        int has_val = (read + 1 < *argc);

        if (!strcmp(a, "--load") && has_val) {
            s_load_name = argv[read + 1];
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--exec") && has_val) {
            copy_arg(s_exec, sizeof(s_exec), argv[read + 1]);
            s_have_exec = 1;
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--exec-at")) {
            char *end = NULL;
            long t;

            /* Cli_ValidateArgs already refuses a run whose --exec-at is missing a value or
               whose value is a flag. Re-check anyway: falling through to the argv passthrough
               below would drop the flag in silence, which is the failure this flag exists to
               avoid. */
            if (read + 2 >= *argc) {
                /* Cli_ValidateArgs rejects this before boot; if it is ever reached anyway,
                   fail rather than boot a partially-parsed command line. */
                fprintf(stderr, "[control] --exec-at: needs <tick> <cmds>\n");
                exit(2);
            }

            t = strtol(argv[read + 1], &end, 0);
            if (end == argv[read + 1] || *end != '\0' || t < 0) {
                fprintf(stderr, "[control] --exec-at: invalid tick '%s' (skipping)\n",
                        argv[read + 1]);
            } else if (s_exec_at_count >= CONTROL_EXEC_AT_MAX) {
                fprintf(stderr, "[control] --exec-at: at most %d entries (skipping '%s')\n",
                        CONTROL_EXEC_AT_MAX, argv[read + 2]);
            } else {
                s_exec_at[s_exec_at_count].tick = t;
                s_exec_at[s_exec_at_count].fired = 0;
                copy_arg(s_exec_at[s_exec_at_count].cmds,
                         sizeof(s_exec_at[s_exec_at_count].cmds), argv[read + 2]);
                s_exec_at_count++;
            }
            s_active = 1;
            read += 3;
            continue;
        }
        if (!strcmp(a, "--listen") && has_val) {
            char *end = NULL;
            long v = strtol(argv[read + 1], &end, 0);
            if (end == argv[read + 1] || *end != '\0' || v < 1 || v > 65535) {
                fprintf(stderr, "[control] --listen: invalid port '%s' (skipping)\n",
                        argv[read + 1]);
            } else {
                s_listen_port = v;
                s_have_listen = 1;
            }
            /* Deliberately not s_active. The socket is a transport: it says how the run
               is driven, not what the run is. Arming here bypassed the menu, so the one
               thing a live driver most wants to reach was the one thing it could not.
               Pass --load or --tick alongside it to get the batch boot path. */
            read += 2;
            continue;
        }
        if (!strcmp(a, "--tick") && has_val) {
            char *end = NULL;
            long v = strtol(argv[read + 1], &end, 0);
            if (end == argv[read + 1] || *end != '\0' || v < 0) {
                fprintf(stderr, "[control] --tick: invalid count '%s' (using 0)\n",
                        argv[read + 1]);
                v = 0;
            }
            s_tick = v;
            s_have_tick = 1;
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--fixed-dt") && has_val) {
            char *end = NULL;
            long v = strtol(argv[read + 1], &end, 0);
            if (end == argv[read + 1] || *end != '\0' || v <= 0) {
                fprintf(stderr, "[control] --fixed-dt: invalid step '%s' (ignoring)\n",
                        argv[read + 1]);
            } else {
                s_fixed_dt = v;
                s_have_fixed_dt = 1;
            }
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--fixed-timestep") && has_val) {
            /* Set the fixed-timestep throttle (FixedTimestep) for this run only, without
               persisting to lba2.cfg (unlike the `fixedtimestep` console verb). Lets a test
               force the throttle on/off + a step size regardless of the local cfg. 0 = off. */
            char *end = NULL;
            long v = strtol(argv[read + 1], &end, 0);
            if (end == argv[read + 1] || *end != '\0' || v < 0 || v > 100) {
                fprintf(stderr, "[control] --fixed-timestep: invalid value '%s' (want 0..100, ignoring)\n",
                        argv[read + 1]);
            } else {
                s_fixed_timestep = v;
                s_have_fixed_timestep = 1;
            }
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--vsync") && has_val) {
            /* Pin DisplayVSync for this run only, without persisting to lba2.cfg (unlike the
               `vsync` console verb and the Display menu's own toggle). The Display submenu
               prints the setting as a row, so a UI capture inherits whatever the local cfg
               holds unless the run says which one it wants.

               The setting, not the frame pacing: Control_Begin drops the presentation cap for
               every --exit run regardless, so pinning this on doesn't put a harness run back
               behind a ~60 fps wait. */
            const char *val = argv[read + 1];
            /* Same spellings the `vsync` console verb takes, so knowing one tells you the
               other. */
            if (!strcasecmp(val, "on") || !strcmp(val, "1")) {
                s_vsync = 1;
                s_have_vsync = 1;
            } else if (!strcasecmp(val, "off") || !strcmp(val, "0")) {
                s_vsync = 0;
                s_have_vsync = 1;
            } else {
                fprintf(stderr, "[control] --vsync: invalid '%s' (use on|off)\n", val);
            }
            read += 2;
            continue;
        }
        if (!strcmp(a, "--dump-state") && has_val) {
            copy_arg(s_dump_path, sizeof(s_dump_path), argv[read + 1]);
            s_have_dump = 1;
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--screenshot") && has_val) {
            copy_arg(s_shot_path, sizeof(s_shot_path), argv[read + 1]);
            s_have_shot = 1;
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--polyrec") && has_val) {
            copy_arg(s_polyrec_path, sizeof(s_polyrec_path), argv[read + 1]);
            s_have_polyrec = 1;
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--capture-projection") && has_val) {
            copy_arg(s_projrec_path, sizeof(s_projrec_path), argv[read + 1]);
            s_have_projrec = 1;
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--projection-hash") && has_val) {
            copy_arg(s_projrec_hash_path, sizeof(s_projrec_hash_path),
                     argv[read + 1]);
            s_have_projrec_hash = 1;
            s_active = 1;
            read += 2;
            continue;
        }
        if (!strcmp(a, "--exit")) {
            s_exit = 1;
            s_active = 1;
            read += 1;
            continue;
        }
        if (!strcmp(a, "--demo")) {
            s_demo = 1;
            s_active = 1;
            read += 1;
            continue;
        }
        if (!strcmp(a, "--verbose")) {
            s_verbose = 1;
            s_active = 1;
            read += 1;
            continue;
        }
        if (!strcmp(a, "--log-level") && has_val) {
            /* Master log level for every sink (file, terminal, F12 console).
               Applied in PERSO.CPP right after Log_Init. Not a harness flag, so
               it doesn't set s_active; like --resolution, it's a normal option. */
            LogSeverity lvl;
            if (Log_ParseLevel(argv[read + 1], &lvl)) {
                s_log_level = lvl;
                s_have_log_level = 1;
            } else {
                fprintf(stderr,
                        "[control] --log-level: unknown level '%s' "
                        "(use debug|info|warn|error)\n",
                        argv[read + 1]);
            }
            read += 2;
            continue;
        }
        if (!strcmp(a, "--resolution") && has_val) {
            /* Format: WxH (or WXH). Width must be a positive multiple of 8 (matches
               the engine's blitter / clip stride assumptions; same constraint as the
               compile-time check in RESOLUTION.H). Height bounded by RES_MAX_H
               (= ADELINE_MAX_Y_RES). */
            const char *val = argv[read + 1];
            const char *xp = strchr(val, 'x');
            if (xp == NULL)
                xp = strchr(val, 'X');
            int parsed = 0;
            if (xp && xp != val && xp[1] != '\0') {
                char *end_w = NULL;
                char *end_h = NULL;
                long w = strtol(val, &end_w, 10);
                long h = strtol(xp + 1, &end_h, 10);
                if (end_w == xp && end_h && *end_h == '\0' &&
                    w >= RES_MIN_W && h >= RES_MIN_H &&
                    w <= RES_MAX_W && h <= RES_MAX_H &&
                    (w & 7) == 0) {
                    s_have_resolution = 1;
                    s_res_x = w;
                    s_res_y = h;
                    parsed = 1;
                }
            }
            if (!parsed) {
                fprintf(stderr,
                        "[control] --resolution: invalid '%s' (need WxH; W%%8==0; "
                        "range %dx%d - %dx%d)\n",
                        val, RES_MIN_W, RES_MIN_H,
                        RES_MAX_W, RES_MAX_H);
            }
            read += 2;
            continue;
        }
        if (!strcmp(a, "--language") && has_val) {
            /* Accept either the full language name (English / Français / Deutsch /
               Español / Italiano / Portugues) or the 2-character code (EN / FR /
               DE / SP / IT / PO).

               Matched with EqualsIgnoringCaseAndEncoding, the same comparison
               InitLanguage() uses on the cfg, because the table above is UTF-8
               (this source file is) while argv is in whatever the console hands
               us: UTF-8 on a modern terminal, but the ANSI codepage from a
               Windows cmd.exe, where Español arrives as one byte for the tilde
               rather than two. A byte-wise strcasecmp only ever matched the
               spelling that happened to share our encoding, which is why the
               ASCII short codes always worked and the accented full names did
               not. */
            const char *val = argv[read + 1];
            int matched = -1;
            for (int n = 0; n < CONTROL_LANG_COUNT; n++) {
                if (EqualsIgnoringCaseAndEncoding(val, kControlLanguageFull[n]) ||
                    !strcasecmp(val, kControlLanguageShort[n])) {
                    matched = n;
                    break;
                }
            }
            if (matched >= 0) {
                s_have_language = 1;
                s_language = matched;
            } else {
                fprintf(stderr,
                        "[control] --language: invalid '%s' (valid: "
                        "English/Français/Deutsch/Español/Italiano/Portugues "
                        "or EN/FR/DE/SP/IT/PO)\n",
                        val);
            }
            read += 2;
            continue;
        }
        if (!strcmp(a, "--res-switch-test") && has_val) {
            /* Format: WxH@TICK (or WXH@TICK).  Width must satisfy the same
               --resolution validator (W%8==0, range RES_MIN_W..RES_MAX_W by
               RES_MIN_H..RES_MAX_H).  TICK is a positive tick count; Res_Switch
               will fire when Control_TickHook's counter passes it. */
            const char *val = argv[read + 1];
            const char *xp = strchr(val, 'x');
            if (xp == NULL)
                xp = strchr(val, 'X');
            const char *at = strchr(val, '@');
            int parsed = 0;
            if (xp && at && xp < at && xp != val && xp[1] != '\0' && at[1] != '\0') {
                char *end_w = NULL, *end_h = NULL, *end_t = NULL;
                long w = strtol(val, &end_w, 10);
                long h = strtol(xp + 1, &end_h, 10);
                long t = strtol(at + 1, &end_t, 10);
                if (end_w == xp && end_h == at && end_t && *end_t == '\0' &&
                    w >= RES_MIN_W && h >= RES_MIN_H &&
                    w <= RES_MAX_W && h <= RES_MAX_H &&
                    (w & 7) == 0 && t > 0) {
                    s_have_res_switch_test = 1;
                    s_res_switch_test_w = w;
                    s_res_switch_test_h = h;
                    s_res_switch_test_tick = t;
                    s_active = 1;
                    parsed = 1;
                }
            }
            if (!parsed) {
                fprintf(stderr,
                        "[control] --res-switch-test: invalid '%s' "
                        "(need WxH@TICK; W%%8==0; range %dx%d - %dx%d; TICK>0)\n",
                        val, RES_MIN_W, RES_MIN_H,
                        RES_MAX_W, RES_MAX_H);
            }
            read += 2;
            continue;
        }
        /* Deliberately not s_active: recording observes a run, it does not drive one.
           Arming the harness here would bypass the menu and boot straight into a scene,
           which is the opposite of what a flag that exists to capture the menu wants. */
        if (!strcmp(a, "--record") && read + 1 < *argc) {
            s_record_path = argv[read + 1];
            read += 2;
            continue;
        }
        if (!strcmp(a, "--replay") && read + 1 < *argc) {
            s_replay_path = argv[read + 1];
            read += 2;
            continue;
        }
        if (!strcmp(a, "--ignore-focus")) {
            s_ignore_focus = 1;
            read += 1;
            continue;
        }
        if (!strcmp(a, "--headless")) {
            /* No window and no audio device: the supported way to run automation. A visible
               window pauses the game on focus loss (HandleEventsTimer) and a live audio thread
               branches the sim, so a windowed run is only reproducible for as long as nothing
               else touches the desktop.

               SDL reads these hints at SDL_Init, so they must be set before it: Control_ParseArgs
               runs first thing in main(), which is why this is parsed here rather than later. */
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
            SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
            s_headless = 1;
            s_no_audio = 1;
            read += 1;
            continue;
        }
        if (!strcmp(a, "--no-autosave")) {
            /* Stop the run autosaving over the player's slots. An in-game scene transition
               calls AutoSaveGame(), so a --load + --tick run that walks the hero through a
               door rewrites autosave.lba on a real install. Explicit saves (menu Save, the
               console's `savebug`) still work: see SAVEGAME.CPP.

               Like --resolution, this describes the environment rather than requesting the
               automated boot path, so it does not set s_active. */
            Save_SetAutoDisabled(1);
            read += 1;
            continue;
        }
        if (!strcmp(a, "--no-audio")) {
            /* Boolean flag — skip InitAIL() and InitSampleDriver() at boot so
               the SDL audio subsystem never opens a stream and the dummy
               driver's nanosleep pacing (~58% of sys time on WSL2) never
               runs. The engine's audio playback paths already gate on
               Sample_Driver_Enabled / non-NULL stream — every call becomes a
               silent no-op when this is set. */
            s_no_audio = 1;
            read += 1;
            continue;
        }

        argv[write++] = argv[read++];
    }
    argv[write] = NULL;
    *argc = write;

    /* A batch run (one that advances ticks or quits on its own) has nobody watching the
       window, so never let focus loss stop its clock.

       --listen joins that set, and
       --ignore-focus states it outright. A socket-driven run has a driver, and the
       driver is in another window by definition, so a windowed --listen session
       froze the moment the operator alt-tabbed to type at it. */
    if (s_headless || s_have_tick || s_exit || s_have_listen || s_ignore_focus) {
        Timer_SetIgnoreFocus(1);
    }

    if (!s_headless && (s_have_tick || s_exit)) {
        fprintf(stderr,
                "[control] warning: batch run without --headless. The window still opens, "
                "and a live audio thread branches the simulation, so results (screenshots, "
                "--dump-state) may not reproduce. Pass --headless.\n");
    }
}

/* Called from main once the CLI is parsed. The recorder starts itself on its first
   input poll, which happens in every mode including the menus. */
void Control_ArmRecording(void) {
    if (s_record_path)
        Record_ArmRecord(s_record_path);
    else if (s_replay_path)
        Record_ArmReplay(s_replay_path);
}

int Control_IsActive(void) {
    return s_active;
}

/* Read by PERSO.CPP: a headless run must not open the modal game-data folder picker,
   which would hang forever with nobody to answer it. */
int Control_IsHeadless(void) {
    return s_headless;
}

int Control_IsVerbose(void) {
    return s_verbose;
}

int Control_HasLogLevel(void) {
    return s_have_log_level;
}

int Control_GetLogLevel(void) {
    return (int)s_log_level;
}

int Control_HasResolution(void) {
    return s_have_resolution;
}

long Control_GetResolutionX(void) {
    return s_res_x;
}

long Control_GetResolutionY(void) {
    return s_res_y;
}

int Control_HasLanguage(void) {
    return s_have_language;
}

int Control_GetLanguage(void) {
    return s_language;
}

int Control_HasFixedTimestep(void) {
    return s_have_fixed_timestep;
}

long Control_GetFixedTimestep(void) {
    return s_fixed_timestep;
}

int Control_HasVSync(void) {
    return s_have_vsync;
}

int Control_GetVSync(void) {
    return s_vsync;
}

int Control_NoAudio(void) {
    return s_no_audio;
}

/* The flag says what was asked for; this says what the run got, and the simulation reads
   the second one.

   Not `Sample_Driver_Enabled`, which answers a question one backend answers misleadingly:
   the null backend initialises, sets the flag, and then returns FALSE from every
   IsSamplePlaying, so reading the flag would report audio on a build with none. The
   backends answer for themselves instead. */
int Control_AudioActive(void) {
    return Sample_DriverPlaysSound() ? 1 : 0;
}

/* --- Boot ------------------------------------------------------------------- */
/* Resolve a --load name into GamePathname. Mirrors the ArgV[1] resolution in main():
   try as a direct path, then the save directory, then with a .lba suffix. */
static int control_resolve_save(const char *name) {
    char tmp[ADELINE_MAX_PATH];

    if (ExistsFileOrDir(name)) {
        copy_arg(GamePathname, ADELINE_MAX_PATH, name);
        return 1;
    }
    GetSavePath(GamePathname, ADELINE_MAX_PATH, name);
    if (ExistsFileOrDir(GamePathname))
        return 1;

    snprintf(tmp, sizeof(tmp), "%s.lba", name);
    GetSavePath(GamePathname, ADELINE_MAX_PATH, tmp);
    if (ExistsFileOrDir(GamePathname))
        return 1;

    return 0;
}

int Control_Begin(void) {
    /* Pin the deterministic clock before the boot/load path runs. With the virtual clock
       frozen (the hook only advances it once MainLoop starts), every ManageTime() during
       InitGame/ChangeCube banks a zero delta, so the base clock stays exactly the restored
       save value -- no boot wall-clock leaks into it. Harness-only; off by default. */
    if (s_have_fixed_dt)
        Timer_EnableFixedDt((U32)s_fixed_dt);

    /* No human is watching a --exit run, so drop the ~60 fps vsync cap and let the loop
       run flat-out. Presentation only -- the rendered pixels and simulation state are
       unchanged (verified: dumps and screenshots are identical with vsync off). */
    Window_SetVSync(0);

    /* Attract/demo mode: the scene's scripts drive the hero along authored tracks (a
       "canned playthrough"), and dialogues/choices auto-advance on the game clock instead
       of waiting for input. BeginDemoSlide() sets DemoSlide = TRUE; that flag routes
       ChangeCube's seeding through Demo_RngSeed (SOURCES/OBJECT.CPP), which seeds from the
       cube number instead of TimerRefHR -- so the demo reel reproduces on every host
       (Linux/macOS/Windows) regardless of how much wall-clock elapses between
       BeginDemoSlide() and the first ChangeCube. Opt-in; default gameplay is unaffected.
       See issue #176. */
    if (s_demo)
        BeginDemoSlide();

    /* Arm projection capture before any InitGame/ChangeCube path that may call
       SetProjection. The boot path's first SetProjection (EXTFUNC.CPP at scene init)
       lands in the capture file once this is open. Hash and full-text sinks are
       independent and composable — either flag (or both) can be given.
       Harness-only; default off. */
    if (s_have_projrec) {
        if (!Projrec_BeginCapture(s_projrec_path))
            fprintf(stderr, "[control] --capture-projection: capture not armed\n");
    }
    if (s_have_projrec_hash) {
        if (!Projrec_BeginHashCapture(s_projrec_hash_path))
            fprintf(stderr, "[control] --projection-hash: hash capture not armed\n");
    }

    if (s_load_name) {
        if (!control_resolve_save(s_load_name)) {
            /* Report and bail without arming. The caller skips MainLoop and exits, so
               we don't depend on TheEnd being noreturn from inside here. */
            fprintf(stderr, "[control] load: save not found: %s\n", s_load_name);
            /* A name resolves against the user save directory, not the retail SAVE/ folder,
               so "not found" is most often a name the engine can't see rather than a typo.
               Name the way to list what it can. */
            fprintf(stderr, "[control] load: a name resolves in the save directory; pass a path,\n"
                            "[control]       or list the names with --exec \"listsaves\".\n");
            fflush(stderr);
            return 0;
        }
        /* Restore exactly as the menu's load_game path does: ChangeCube must run
           while FlagLoadGame is still set (InitGame(-1) sets it; MainLoop clears it
           at its top), otherwise the saved hero position is not restored. */
        InitGame(-1);
        ChangeCube();
        FlagFade = FALSE;
        /* Upload the scene palette by hand, because clearing FlagFade above is
           what stops it happening on its own. Nothing in the engine sets the
           video palette as its own step: the menu's load path gets it from
           ZoomSavedGame's closing PaletteSync, and the in-game path gets it from
           the fade-in FlagFade arms. A batch run wants neither animation, so
           both palette uploads go with them and every present maps a correctly
           composed Log through an all-black table -- a black window, with audio
           and simulation running normally.

           Invisible to the harness's own eyes, which is why it could sit here:
           --shot writes SavePNG(Log, PtrPal), reading the game's palette rather
           than the video one, so screenshots came out correct while the window
           showed nothing. Only a readback of the presented frame separates the
           two. */
        Palette(PtrPal);
    } else {
        /* Fresh start; mirrors the proven debug cube-select path (InitGame(1) then
           MainLoop, whose first iteration loads NewCube == 0). */
        InitGame(1);
    }

#ifndef ENABLE_POLY_RECORDING
    if (s_have_polyrec)
        fprintf(stderr, "[control] --polyrec ignored: built without ENABLE_POLY_RECORDING\n");
#endif

    s_hook_calls = 0;
    s_finalized = 0;
    s_armed = 1;
    return 1;
}

void Control_DumpStateTo(const char *path) {
    if (!path || !path[0])
        return;
    control_dump_state(path);
}

void Control_ServiceListen(void) {
    if (!s_have_listen)
        return;
    /* Opened on the first present rather than the first tick: the menu and every
       cinematic run their own loop, so a listener that waited for MainLoop would
       never open at all for a run that starts at the menu. Both calls below are
       non-blocking: with nobody connected this is one failed accept(). */
    if (!s_listen_started) {
        s_listen_started = 1;
        ControlServer_Start((int)s_listen_port);
    }
    ControlServer_Poll();
}

/* --- Tick driver ------------------------------------------------------------ */
static long control_effective_ticks(void) {
    long n = s_have_tick ? s_tick : 0;
    if ((s_have_shot || s_have_polyrec) && n < 1)
        n = 1; /* a rendered artifact needs at least one completed AffScene */
    return n;
}

static void control_run_commands(const char *cmds) {
    char buf[CONTROL_EXEC_MAX];
    char *p, *seg;

    if (!cmds || !cmds[0])
        return;

    /* Mirror console output to stdout so --exec results are observable / testable. */
    Console_SetLineSinkForTests(control_console_echo);
    Console_EnsureRegistered();

    copy_arg(buf, sizeof(buf), cmds);
    p = buf;
    while (p && *p) {
        seg = p;
        p = strchr(p, ';');
        if (p)
            *p++ = '\0';
        /* trim */
        while (*seg == ' ' || *seg == '\t')
            seg++;
        {
            size_t len = strlen(seg);
            while (len > 0 && (seg[len - 1] == ' ' || seg[len - 1] == '\t'))
                seg[--len] = '\0';
        }
        if (*seg)
            Console_Execute(seg);
    }
}

static void control_run_exec(void) {
    if (!s_have_exec)
        return;
    control_run_commands(s_exec);
}

/* Fire any --exec-at whose tick has come. `tick` is 0-based, matching --verbose and
   --dump-state, so --exec and --exec-at 0 are the same moment. Compares with >= rather
   than == so a batch that starts past the requested tick still runs it once. */
static void control_run_exec_at(long tick) {
    int i;
    for (i = 0; i < s_exec_at_count; i++) {
        if (!s_exec_at[i].fired && tick >= s_exec_at[i].tick) {
            s_exec_at[i].fired = 1;
            control_run_commands(s_exec_at[i].cmds);
        }
    }
}

static void control_console_echo(const char *line) {
    printf("[control] %s\n", line);
}

/* --- State dump ------------------------------------------------------------- */
static void json_str(FILE *f, const char *s) {
    fputc('"', f);
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c == '\n') {
            fputs("\\n", f);
        } else if (c == '\t') {
            fputs("\\t", f);
        } else if (c < 0x20) {
            fprintf(f, "\\u%04x", c);
        } else {
            fputc(c, f);
        }
    }
    fputc('"', f);
}

static void control_dump_state(const char *path) {
    FILE *f = fopen(path, "wb");
    T_OBJET *hero;
    long ticks_done;

    if (!f) {
        fprintf(stderr, "[control] dump-state: cannot open %s\n", path);
        return;
    }

    hero = &ListObjet[NUM_PERSO];
    ticks_done = s_hook_calls > 0 ? s_hook_calls - 1 : 0;

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": 1,\n");
    fprintf(f, "  \"tick\": %ld,\n", ticks_done);
    fprintf(f, "  \"timer_ref_hr\": %u,\n", (unsigned int)TimerRefHR);
    fprintf(f, "  \"fps\": %d,\n", (int)NbFramePerSecond);
    fprintf(f, "  \"throws\": %d,\n", (int)DbgHeroThrows);
    fprintf(f, "  \"action_resets\": %d,\n", (int)DbgHeroActionResets);

    /* Per-boundary input flow (SYSTEM/INPUT_FLOW.H). Counts, not rates: a
       reader compares the two sides of a boundary and the gap is what that
       layer lost. Cumulative for the run, so a harness reads it once at the
       end rather than sampling. */
    fprintf(f,
            "  \"input_flow\": { \"key_down_events\": %u, \"key_up_events\": %u, "
            "\"polled_rises\": %u, \"polls\": %u, "
            "\"tabkey_rises\": %u, \"input_rises\": %u, \"input_rises_masked\": %u, "
            "\"frames_with_edge\": %u, \"frames_with_edge_skipped\": %u, "
            "\"action_edges_skipped\": %u },\n",
            (unsigned)InputFlow.KeyDownEvents, (unsigned)InputFlow.KeyUpEvents,
            (unsigned)InputFlow.PolledRises, (unsigned)InputFlow.PollsSampled,
            (unsigned)InputFlow.TabKeyRises, (unsigned)InputFlow.InputRises,
            (unsigned)InputFlow.InputRisesMasked,
            (unsigned)InputFlow.FramesWithEdge, (unsigned)InputFlow.FramesWithEdgeSkipped,
            (unsigned)InputFlow.ActionEdgesSkipped);

    fprintf(f,
            "  \"scene\": { \"island\": %d, \"cube\": %d, \"cube_mode\": \"%s\", "
            "\"num_objects\": %d, \"num_zones\": %d, \"num_tracks\": %d, \"num_patches\": %d, "
            "\"render_w\": %u, \"render_h\": %u },\n",
            (int)Island, (int)NumCube, CubeMode == CUBE_INTERIEUR ? "interior" : "exterior",
            (int)NbObjets, (int)NbZones, (int)NbBrickTrack, (int)NbPatches,
            (unsigned)ModeDesiredX, (unsigned)ModeDesiredY);

    fprintf(f,
            "  \"hero\": { \"x\": %d, \"y\": %d, \"z\": %d, "
            "\"alpha\": %d, \"beta\": %d, \"gamma\": %d, "
            "\"life\": %d, \"behaviour\": %d, \"weapon\": %d, "
            "\"body\": %d, \"anim\": %d, \"gen_anim\": %d, \"last_frame\": %d, \"nb_frames\": %d, "
            "\"move\": %d, \"zone_sce\": %d, \"flags\": %u, \"work_flags\": %u },\n",
            (int)hero->Obj.X, (int)hero->Obj.Y, (int)hero->Obj.Z,
            (int)hero->Obj.Alpha, (int)hero->Obj.Beta, (int)hero->Obj.Gamma,
            (int)hero->LifePoint, (int)Comportement, (int)Weapon,
            (int)hero->Obj.Body.Num, (int)hero->Obj.Anim.Num, (int)hero->GenAnim,
            (int)hero->Obj.LastFrame, (int)hero->Obj.NbFrames,
            (int)hero->Move, (int)hero->ZoneSce, (unsigned int)hero->Flags,
            (unsigned int)hero->WorkFlags);

    /* Where the camera is and who is deciding that. `zone`, `forced` and `cine` say
     * whether an authored shot or a cutscene holds the view rather than the player's
     * own camera, which is the difference between a fixture asserting on what it set
     * up and one asserting on a shot it never noticed taking over. */
    fprintf(f,
            "  \"camera\": { \"beta\": %d, \"alpha\": %d, \"gamma\": %d, \"add_beta\": %d, "
            "\"distance\": %d, \"offset_x\": %d, \"offset_y\": %d, \"offset_z\": %d, "
            "\"follow\": %d, \"hold_angle\": %d, \"base_dist\": %d, \"view\": %d, "
            "\"zone\": %d, \"forced\": %d, \"cine\": %d },\n",
            (int)BetaCam, (int)AlphaCam, (int)GammaCam, (int)AddBetaCam,
            (int)VueDistance, (int)VueOffsetX, (int)VueOffsetY, (int)VueOffsetZ,
            (int)FollowCamera, (int)FollowCamHoldAngle, (int)FollowCamBaseDist, (int)VueCamera,
            (int)CameraZone, (int)FlagCameraForcee, (int)CinemaMode);

    fprintf(f,
            "  \"inventory\": { \"magic_level\": %d, \"magic_point\": %d, "
            "\"gold\": %d, \"zlitos\": %d, \"keys\": %d, \"clover_boxes\": %d },\n",
            (int)MagicLevel, (int)MagicPoint, (int)NbGoldPieces, (int)NbZlitosPieces,
            (int)NbLittleKeys, (int)NbCloverBox);

    fprintf(f, "  \"actors\": [");
    {
        S32 i;
        for (i = 0; i < NbObjets; i++) {
            T_OBJET *o = &ListObjet[i];
            fprintf(f,
                    "%s\n    { \"index\": %d, \"x\": %d, \"y\": %d, \"z\": %d, \"beta\": %d, "
                    "\"life\": %d, \"body\": %d, \"anim\": %d, \"move\": %d, \"flags\": %u, "
                    "\"armure\": %d, \"nb_bonus\": %d }",
                    i ? "," : "", (int)i,
                    (int)o->Obj.X, (int)o->Obj.Y, (int)o->Obj.Z, (int)o->Obj.Beta,
                    (int)o->LifePoint, (int)o->Obj.Body.Num, (int)o->Obj.Anim.Num,
                    (int)o->Move, (unsigned int)o->Flags,
                    /* What separates a combatant from a body standing in a room.
                       Armure is damage resistance; NbBonus is what it drops when
                       killed. Neither is proof on its own, which is why the survey
                       that uses them confirms a candidate by hitting it. */
                    (int)o->Armure, (int)o->NbBonus);
        }
    }
    fprintf(f, "%s],\n", NbObjets ? "\n  " : "");

    fprintf(f, "  \"vars\": { \"count\": %d, \"nonzero\": {", (int)MAX_VARS_GAME);
    {
        int i, first = 1;
        for (i = 0; i < MAX_VARS_GAME; i++) {
            if (ListVarGame[i] != 0) {
                fprintf(f, "%s \"%d\": %d", first ? "" : ",", i, (int)ListVarGame[i]);
                first = 0;
            }
        }
    }
    fprintf(f, " } },\n");

    /* Scene (cube) Life variables. Together with the game vars above these are the quest state,
       so a corpus of saves dumped through this is a timeline of quest progression. */
    fprintf(f, "  \"vars_cube\": { \"count\": %d, \"nonzero\": {", (int)MAX_VARS_CUBE);
    {
        int i, first = 1;
        for (i = 0; i < MAX_VARS_CUBE; i++) {
            if (ListVarCube[i] != 0) {
                fprintf(f, "%s \"%d\": %d", first ? "" : ",", i, (int)ListVarCube[i]);
                first = 0;
            }
        }
    }
    fprintf(f, " } },\n");

    fprintf(f, "  \"log\": [");
    {
        const char *lines[16];
        int n = Console_GetScrollback(lines, 16);
        int i;
        for (i = 0; i < n; i++) {
            fprintf(f, "%s\n    ", i ? "," : "");
            json_str(f, lines[i]);
        }
        fprintf(f, "%s]\n", n ? "\n  " : "");
    }

    fprintf(f, "}\n");
    fclose(f);
}

static void control_screenshot(const char *path) {
    /* Capture Log, the presented buffer (SDL PresentFrame uploads Log). Screen is
       a secondary buffer that only holds the clean brick background, so it misses
       the per-frame 3D models and the foreground-brick re-blit. */
    SavePNG((char *)path, Log, (S32)ModeDesiredX, (S32)ModeDesiredY, PtrPal);
}

/* The capture half of finalize: dump/screenshot only, no exit. Shared by the tick-budget
   path (control_finalize) and the natural-return path (Control_End). Idempotent. */
static void control_capture(void) {
    if (s_finalized)
        return;
    s_finalized = 1;

    if (s_have_dump)
        control_dump_state(s_dump_path);
    if (s_have_shot)
        control_screenshot(s_shot_path);
    /* Flush projection capture last so any final-frame SetProjection from the dump/
       screenshot present path is included. End is idempotent; safe even if never armed,
       and closes both sinks in one call. */
    if (s_have_projrec || s_have_projrec_hash)
        Projrec_EndCapture();
}

static void control_finalize(void) {
    control_capture();

    /* The tick budget is spent, so the hook that polls the socket is about to stop
       running. Close it rather than leave a listener nothing will ever answer. */
    ControlServer_Stop();

    if (s_exit) {
        /* An --exec-at whose tick the run never reached did not do what the command line
           asked for, so it must not pass for a clean run. This is where an --exit run ends,
           so the check belongs here as well as on main()'s natural-end path. */
        int unfired = Control_UnfiredExecAt();

        fflush(stdout);
        TheEnd(unfired > 0 ? PROGRAM_FAIL : PROGRAM_OK,
               unfired > 0 ? "control: --exec-at never ran" : "");
    }
    /* Not exiting: stop mirroring console output (the game keeps running and would
       otherwise echo every later console line), disarm, and hand control back. */
    Console_SetLineSinkForTests(NULL);
    s_armed = 0;
}

void Control_End(void) {
    /* MainLoop returned without hitting the --tick budget (e.g. LM_THE_END at cube 218
       on the attract reel). Capture the dump/screenshot the caller asked for; main()
       will then call TheEnd() as it does for any harness run. No-op if the tick-budget
       path already finalized, the harness was never active, or no capture was asked. */
    if (!s_active || s_finalized)
        return;
    if (!s_have_dump && !s_have_shot)
        return;
    control_capture();
}

int Control_UnfiredExecAt(void) {
    int i, n = 0;

    /* An --exec-at whose tick the run never reached did not do what the command line asked
       for, so it must not pass for a clean run. Report each one and let the caller fail. */
    for (i = 0; i < s_exec_at_count; i++) {
        if (!s_exec_at[i].fired) {
            fprintf(stderr,
                    "[control] --exec-at %ld never ran: the run ended at tick %ld. "
                    "Raise --tick, or schedule it earlier.\n",
                    s_exec_at[i].tick, s_hook_calls > 0 ? s_hook_calls - 1 : 0);
            n++;
        }
    }
    fflush(stderr);
    return n;
}

void Control_TickHook(void) {
    /* Ahead of the armed check: a player recording their own session has passed no
       harness flag at all, so the hook is otherwise dead for exactly the run that
       matters most. */
    Record_TickHook();
    /* The recorder arms the fixed-dt clock itself for a mid-session `rec start`, and
       nothing else was advancing it. s_have_fixed_dt is set by --fixed-dt and by nothing
       else, and the advance that reads it sits below the armed check, so a run that armed
       the step any other way was left with a virtual clock that was armed and frozen.
       Game time stopped: the hero stopped walking, and the recording still replayed clean
       because both ends agreed about a stopped game. The step comes from whoever armed
       it, so this cannot double-advance the harness path -- record_reload_issue only arms
       when Timer_FixedDtActive() is false. */
    if (!s_have_fixed_dt && Timer_FixedDtActive())
        Timer_FixedDtAdvance();
    if (!s_armed)
        return;
    s_hook_calls++;
    /* Apply --fixed-timestep once, on the first tick: it must land after ReadConfigFile (which
       set FixedTimestep from lba2.cfg during boot) so the flag overrides the cfg for this run
       only, without persisting. */
    if (s_have_fixed_timestep && s_hook_calls == 1)
        FixedTimestep = (S32)s_fixed_timestep;
    /* Advance the deterministic clock one fixed step per tick. The hook runs exactly once
       per loop-body iteration, before this tick's ManageTime() banks the step. */
    if (s_have_fixed_dt)
        Timer_FixedDtAdvance();
    /* --verbose: log the scene/hero state to stderr when it changes, so a headless run is
       self-explaining (which scene, what the hero is doing) and the last line before a hang
       shows where the loop got stuck. The hook runs once per tick; a modal (cinematic/dialogue)
       blocks the loop, so the modal markers below announce those before they hang. */
    if (s_verbose) {
        static S32 last_cube = -2, last_beh = -2, last_anim = -2, last_move = -2;
        T_OBJET *h = &ListObjet[NUM_PERSO];
        /* A periodic heartbeat (every 250 ticks) on top of the change-triggered line: it
           proves the main loop is still ticking through a long idle/cinematic, and
           distinguishes "loop alive but state static" from "stuck in a modal" (no hook). */
        if (NumCube != last_cube || Comportement != last_beh || h->Obj.Anim.Num != last_anim ||
            h->Move != last_move || ((s_hook_calls - 1) % 250) == 0) {
            fprintf(stderr, "[control] t=%-4ld cube=%-3d beh=%d anim=%d move=%d hero=(%d,%d)\n",
                    s_hook_calls - 1, (int)NumCube, (int)Comportement, (int)h->Obj.Anim.Num,
                    (int)h->Move, (int)h->Obj.X, (int)h->Obj.Z);
            last_cube = NumCube;
            last_beh = Comportement;
            last_anim = h->Obj.Anim.Num;
            last_move = h->Move;
        }
    }
    if (s_hook_calls == 1)
        control_run_exec();
    control_run_exec_at(s_hook_calls - 1);

    /* --demo only drives an authored demo scene (the reel the menu enters at cube 193,
       GAMEMENU.CPP; see docs/SCENES.md). On a normal scene it silently does nothing, so a
       long run looks like evidence when it is just an idle hero. Checked a few ticks in,
       once an --exec "cube N" has actually applied (a cube change lands the NEXT frame). */
    if (s_demo && s_hook_calls == 3) {
        if (NumCube < DEMO_CUBE_FIRST || NumCube > DEMO_CUBE_LAST) {
            fprintf(stderr,
                    "[control] warning: --demo has no effect on cube %d. The demo reel is "
                    "cubes %d-%d; jump to one, e.g. --exec \"cube %d\".\n",
                    (int)NumCube, DEMO_CUBE_FIRST, DEMO_CUBE_LAST, DEMO_CUBE_FIRST);
        }
    }
    /* --res-switch-test fires once at the requested tick.  Logged so harness
       output captures success / failure; the orchestrator also logs internally
       on rebuild failure. */
    if (s_have_res_switch_test && !s_res_switch_test_fired &&
        s_hook_calls >= s_res_switch_test_tick) {
        s_res_switch_test_fired = 1;
        bool ok = Res_Switch((U32)s_res_switch_test_w, (U32)s_res_switch_test_h);
        fprintf(stderr, "[control] --res-switch-test fired at tick %ld: %s\n",
                s_hook_calls, ok ? "OK" : "FAIL");
    }
#ifdef ENABLE_POLY_RECORDING
    /* Arm the existing draw-call recorder for the final rendered tick, so the capture
       reflects the same settled frame as --dump-state / --screenshot. */
    if (s_have_polyrec && s_hook_calls == control_effective_ticks()) {
        SnapshotName = s_polyrec_path;
        SnapshotRequested = 1;
    }
#endif
    /* A --listen run with no tick budget is driven by whoever connects, so it must
       not end itself: without this the default budget of zero finalizes on the
       first tick, disarming the hook that services the socket. An explicit --tick
       still wins, because that is the caller asking for a bounded run.
       Only while there is actually a socket to be driven from. The flag parses on
       every build, and the listener is a stub without LBA2_CONTROL_SERVER and can
       fail on a taken port, so keying this on the flag alone left `--listen 4444
       --exit` waiting forever for a driver that could never arrive. Before the
       first attempt nothing is known yet, and ending the run there would be worse
       than waiting one frame. */
    if (s_have_listen && !s_have_tick && (!s_listen_started || ControlServer_IsRunning()))
        return;

    /* Same for a run that is recording or replaying with no tick budget: it is driven by
       whoever is playing, and finalizing here would disarm the hook that calls
       Timer_FixedDtAdvance. Under --fixed-dt that is what makes a tick worth exactly one
       step (it banks the step and lets the tick's own present through for free); without
       it the virtual clock is left to the present path alone, so a tick that presents
       nothing banks nothing. The recording then carries a stalled tick that a replay,
       which does keep its hook armed, never reproduces, and the two run one step apart
       from there on. Measured on a real play session: one stall at tick 2 and a state
       mismatch at tick 14.
       Not when --exit was asked for: that is the caller asking for a bounded run, and
       without the test `--record out.rec --exit` would wait for a quit that never comes. */
    if ((Record_IsRecording() || Record_IsReplaying()) && !s_have_tick && !s_exit)
        return;

    if ((s_hook_calls - 1) >= control_effective_ticks())
        control_finalize();
}

/* ===== Harness input timeline (docs/plan/INPUT_SIM_PLAN.md) ====================== */

#define CONTROL_INPUT_MAX_EVENTS 16

typedef struct {
    U32 start;      /* first clock value the event is held (inclusive) */
    U32 end;        /* first clock value it is released (exclusive)    */
    U32 mask;       /* input bits held over [start, end)               */
    int frameBased; /* 0 = start/end measured in sim ticks, 1 = in rendered frames */
} T_ControlInputEvent;

static T_ControlInputEvent s_input_events[CONTROL_INPUT_MAX_EVENTS];
static int s_input_count = 0;
static U32 s_input_tick = 0;       /* sim-tick clock: advances only on stepped frames */
static U32 s_input_frame = 0;      /* rendered-frame clock: advances every rendered frame */
static U32 s_input_last = 0;       /* max end across sim-tick events */
static U32 s_input_last_frame = 0; /* max end across rendered-frame events */
static int s_input_trace = 0;

void Control_InputClear(void) {
    s_input_count = 0;
    s_input_tick = 0;
    s_input_frame = 0;
    s_input_last = 0;
    s_input_last_frame = 0;
}

static int control_input_schedule(U32 start, U32 dur, U32 mask, int frameBased) {
    if (mask == 0 || dur == 0)
        return 1; /* empty gap: nothing to hold */
    if (s_input_count >= CONTROL_INPUT_MAX_EVENTS)
        return 0;
    T_ControlInputEvent *e = &s_input_events[s_input_count++];
    e->start = start;
    e->end = start + dur;
    e->mask = mask;
    e->frameBased = frameBased;
    if (frameBased) {
        if (e->end > s_input_last_frame)
            s_input_last_frame = e->end;
    } else {
        if (e->end > s_input_last)
            s_input_last = e->end;
    }
    return 1;
}

int Control_InputSchedule(U32 startTick, U32 durTicks, U32 mask) {
    return control_input_schedule(startTick, durTicks, mask, 0);
}

int Control_InputScheduleFrame(U32 startFrame, U32 durFrames, U32 mask) {
    return control_input_schedule(startFrame, durFrames, mask, 1);
}

void Control_InputHold(U32 mask, U32 durTicks) {
    Control_InputClear();
    Control_InputSchedule(0, durTicks, mask);
}

U32 Control_InputCurrentMask(void) {
    U32 m = 0;
    int i;
    for (i = 0; i < s_input_count; i++) {
        U32 clock = s_input_events[i].frameBased ? s_input_frame : s_input_tick;
        if (clock >= s_input_events[i].start && clock < s_input_events[i].end)
            m |= s_input_events[i].mask;
    }
    return m;
}

int Control_InputPending(void) {
    if (s_input_count == 0)
        return 0;
    return s_input_tick < s_input_last || s_input_frame < s_input_last_frame;
}

/* ===== Harness key holds (docs/plan/INPUT_SIM_PLAN.md) ==========================
 *
 * The timeline above ORs its mask into Input from MainLoop, which is everything a
 * test driving the hero needs and nothing a test driving a MENU can use: a modal
 * spins in its own MyGetInput loop and never returns to MainLoop until it is done.
 * These hold a raw scancode in TabKeys instead, from the ApplyHarnessKeys hook that
 * every ManageKeyboard runs, so the press reaches whichever loop is asking.
 *
 * Metered in input polls, one per ManageKeyboard, rather than ticks: a modal advances
 * no ticks, so a tick-based delay inside one would never elapse. Note that a poll is
 * NOT the same as a MyGetInput call. InitWaitNoInput(x) is GetInput(x) (INPUT.H), and
 * every one of those runs ManageKeyboard too, so a path that arms and then latches
 * consumes more polls than it makes MyGetInput calls. In MainLoop, where nothing else
 * is polling, a poll is a rendered frame.
 */

#define CONTROL_KEY_MAX_HOLDS 8

typedef struct {
    U32 scancode;
    U32 start; /* first poll the key is down (inclusive) */
    U32 end;   /* first poll it is up (exclusive)        */
} T_ControlKeyHold;

static T_ControlKeyHold s_key_holds[CONTROL_KEY_MAX_HOLDS];
static int s_key_hold_count = 0;
static U32 s_key_poll = 0;

/* Set or clear a scancode in TabKeys, with the index/bit split CheckKey uses: a byte
   per key below 256, packed bits above for the joystick tail. */
static void control_key_poke(U32 sc, int down) {
    if (sc < 256) {
        if (down)
            TabKeys[sc] |= 0x80;
        else
            TabKeys[sc] &= (U8)~0x80;
    } else {
        U8 bit = (U8)(1 << (sc & 7));
        U32 idx = 256 - 32 + (sc >> 3);
        if (down)
            TabKeys[idx] |= bit;
        else
            TabKeys[idx] &= (U8)~bit;
    }
}

void Control_KeyClear(void) {
    int i;
    /* Lift anything still down. The tail especially: UpdateKeyboardState only memsets
       TabKeys[0..255], so a pad bit left set outlives the clear until the next GetJoys. */
    for (i = 0; i < s_key_hold_count; i++)
        control_key_poke(s_key_holds[i].scancode, 0);
    s_key_hold_count = 0;
}

U32 Control_KeyPollCount(void) {
    return s_key_poll;
}

int Control_KeyHold(U32 scancode, U32 delayPolls, U32 durPolls) {
    int r, w = 0;

    /* Scancode 0 is SDL_SCANCODE_UNKNOWN, and it is also the filler in every unset
       DefKeys/GamepadKeys Key2 slot, so holding it would light up most of the input
       table at once. Reject rather than surprise the caller. */
    if (scancode == 0 || scancode >= CONTROL_KEY_MAX_SCANCODE || durPolls == 0)
        return 0;

    /* Drop elapsed holds so a test that arms repeatedly cannot fill the table. */
    for (r = 0; r < s_key_hold_count; r++)
        if (s_key_holds[r].end > s_key_poll)
            s_key_holds[w++] = s_key_holds[r];
    s_key_hold_count = w;

    if (s_key_hold_count >= CONTROL_KEY_MAX_HOLDS)
        return 0;

    s_key_holds[s_key_hold_count].scancode = scancode;
    /* +1 because s_key_poll counts polls already taken: the earliest poll this hold can
       be seen on is the next one. Without it a delay of 0 loses its first poll and a
       one-poll hold never fires at all. */
    s_key_holds[s_key_hold_count].start = s_key_poll + delayPolls + 1;
    s_key_holds[s_key_hold_count].end = s_key_poll + delayPolls + 1 + durPolls;
    s_key_hold_count++;
    return 1;
}

/* The two analog devices, on the same poll clock as the key holds above.
 *
 * The analog camera reads MouseXDep/MouseYDep, Click and the cached stick axes. None of
 * them is a scancode, so the key holds cannot reach any of them: a headless run had no
 * way to orbit with either device, and a recording of one carried an analog block that
 * nothing ever filled.
 *
 * Both write where the device writes, one step above what reads them: the mouse into the
 * accumulator ManageMouse drains, the stick into the axes UpdateJoystick caches. The
 * drain, the recorder and every consumer then see what they see for a device, in the same
 * order. `camnudge` stands in one step further down, at the nudge the camera applies, so
 * it drives the camera without touching a device; these drive the devices. */

/* A hold measured in input polls, half-open: live from `start` up to but not including
   `end`. An `end` of 0 is "nothing armed", which no live span can be, because arming
   always lands at least one poll ahead. The key holds keep the same pair inline, one per
   entry, because several of them run at once; these two have one each. */
typedef struct {
    U32 start;
    U32 end;
} T_ControlSpan;

typedef enum {
    SPAN_IDLE = 0, /* nothing armed */
    SPAN_PENDING,  /* armed, not started */
    SPAN_LIVE,     /* running this poll */
    SPAN_ENDED     /* ran, and this is the poll after */
} T_ControlSpanPhase;

static void span_arm(T_ControlSpan *span, U32 durPolls) {
    /* +1 because s_key_poll counts polls already taken, so the earliest a hold can be
       seen on is the next one. Without it a one-poll hold never fires at all. */
    span->start = s_key_poll + 1;
    span->end = s_key_poll + 1 + durPolls;
}

static T_ControlSpanPhase span_phase(const T_ControlSpan *span) {
    if (span->end == 0)
        return SPAN_IDLE;
    if (s_key_poll >= span->end)
        return SPAN_ENDED;
    if (s_key_poll < span->start)
        return SPAN_PENDING;
    return SPAN_LIVE;
}

static S32 s_mouse_dx = 0, s_mouse_dy = 0;
static U32 s_mouse_buttons = 0;
static T_ControlSpan s_mouse_span;

void Control_MouseClear(void) {
    s_mouse_dx = s_mouse_dy = 0;
    s_mouse_span.end = 0;
    /* Lift the buttons rather than leaving them for something else to notice: Click is a
       level, not an edge, and nothing clears it but a button-up event that will never
       come for one this put down. */
    if (s_mouse_buttons)
        Click &= (S32)~s_mouse_buttons;
    s_mouse_buttons = 0;
}

int Control_MouseMove(S32 dx, S32 dy, U32 buttons, U32 durPolls) {
    if (durPolls == 0)
        return 0;
    Control_MouseClear();
    s_mouse_dx = dx;
    s_mouse_dy = dy;
    s_mouse_buttons = buttons;
    span_arm(&s_mouse_span, durPolls);
    return 1;
}

static void control_mouse_apply(void) {
    S32 dx, dy;

    switch (span_phase(&s_mouse_span)) {
    case SPAN_ENDED:
        Control_MouseClear();
        return;
    case SPAN_LIVE:
        break;
    default:
        return;
    }

    PeekMouseMotion(&dx, &dy);
    SetMouseMotion(dx + s_mouse_dx, dy + s_mouse_dy);
    Click |= (S32)s_mouse_buttons;
}

static S16 s_stick_x = 0, s_stick_y = 0;
static T_ControlSpan s_stick_span;

void Control_StickClear(void) {
    s_stick_x = s_stick_y = 0;
    s_stick_span.end = 0;
    Joystick_SetVirtualPad(0);
}

int Control_StickHold(S16 x, S16 y, U32 durPolls) {
    if (durPolls == 0)
        return 0;
    Control_StickClear();
    s_stick_x = x;
    s_stick_y = y;
    span_arm(&s_stick_span, durPolls);
    return 1;
}

static void control_stick_apply(void) {
    switch (span_phase(&s_stick_span)) {
    case SPAN_ENDED:
        Control_StickClear();
        return;
    case SPAN_LIVE:
        break;
    default:
        return;
    }

    /* UpdateJoystick clears the cached axes on every poll before this runs, so this
       re-applies on each rather than setting once, which is what a real pad's poll does.
       Presence comes with it: the analog camera asks whether a pad is there before it
       reads the axes, so without it the values would land in a run that never looks. */
    Joystick_SetVirtualPad(1);
    Joystick_SetRightStick(s_stick_x, s_stick_y);
}

/* Strong override of the weak hook in LIB386/SYSTEM/KEYBOARD.CPP. Runs inside
   UpdateKeyboardState, after it has rebuilt TabKeys[0..255] from SDL and after
   GetJoys has written the joystick tail, and before GetInput's CheckKey scan. */
extern "C" void ApplyHarnessKeys(void) {
    int i, w = 0;

    s_key_poll++;
    control_mouse_apply();
    control_stick_apply();

    for (i = 0; i < s_key_hold_count; i++) {
        U32 sc = s_key_holds[i].scancode;

        if (s_key_poll >= s_key_holds[i].end) {
            /* Expired. Lift it explicitly rather than just stopping: below 256 the next
               UpdateKeyboardState memset would do it, but the tail is cleared only by
               ClearGamepadBits inside UpdateJoystick, which runs from GetJoys and so only
               on a MyGetInput. A bare GetInput (InitWaitNoInput) in between would still
               read the bit as held. */
            control_key_poke(sc, 0);
            continue; /* and drop it from the table */
        }

        s_key_holds[w++] = s_key_holds[i];

        if (s_key_poll < s_key_holds[i].start)
            continue;

        control_key_poke(sc, 1);

        /* UpdateKeyboardState takes the first held scancode into Key, which MyGetInput
           passes to MyKey; mirror that so the MyKey == K_ESC paths see an injected Esc.
           Pad buttons deliberately do not: MyGetInput sources those through
           JoystickFirstPressedScancode, and their scancodes never land in Key. */
        if (Key == 0 && sc < 256)
            Key = (S32)sc;
    }

    s_key_hold_count = w;
}

void Control_InputTrace(int on) {
    s_input_trace = on ? 1 : 0;
}

/* Advance the rendered-frame clock. Call once per rendered frame (including throttle-skipped ones)
   so a frame-based event -- a press placed on a specific skip frame -- lands exactly there. */
void Control_InputFrameTick(void) {
    if (Control_InputPending())
        s_input_frame++;
}

void Control_InputAdvance(int simTicks) {
    if (!Control_InputPending())
        return;
    if (s_input_trace) {
        T_OBJET *hero = &ListObjet[NUM_PERSO];
        Log_Debug("[input] tick %u frame %u mask 0x%X hero gen_anim=%d pos=(%d,%d,%d)",
                  (unsigned)s_input_tick, (unsigned)s_input_frame, (unsigned)Control_InputCurrentMask(),
                  (int)hero->GenAnim, (int)hero->Obj.X, (int)hero->Obj.Y, (int)hero->Obj.Z);
    }
    if (simTicks > 0)
        s_input_tick += (U32)simTicks;
}

/* Filled by Control_StateDigest on every call, whether or not anything wants the
   values: a replay needs its own copy to compare against, and collecting costs less
   than the hashing it rides along with. */
#define TELE_MAX (64 + 9 * MAX_OBJETS + MAX_VARS_GAME + MAX_VARS_CUBE)
static S32 s_tele[TELE_MAX];
static const char *s_teleName[TELE_MAX];
static S16 s_teleIdx[TELE_MAX];
static S32 s_teleN = 0;
/* ---- the game state the harness reads ------------------------------------- */
/*
 * Reading the engine's state belongs here rather than in whatever wants it. This module
 * already reaches for the globals to write --dump-state, and it is the one place that
 * includes C_EXTERN.H for that purpose; a second module doing the same would double a
 * dependency the build graph deliberately holds down (scripts/ci/check-build-graph.py
 * rule 2). So the recorder asks for a digest, a keyframe and a state description, and
 * never sees a game global.
 *
 * Three views of the same walk, which is why they share one:
 *   - a 64-bit digest, for "did this tick match"
 *   - the values behind it, named, for "which field moved"
 *   - a short keyframe of the fields a person reads first
 */

static unsigned long long s_h;
static void mix(const void *p, unsigned n) {
    const unsigned char *b = (const unsigned char *)p;
    unsigned i;
    for (i = 0; i < n; i++) {
        s_h ^= b[i];
        s_h *= 1099511628211ULL;
    }
}
/* Mix a value into the digest and keep it, named, for the telemetry. The name is the
   expression itself, which is what stops the two drifting apart: a field cannot be
   hashed under one name and reported under another, and a field added to the digest is
   named without anyone remembering to name it. Scalars carry index -1; the loops below
   pass the array index they are on. */
#define M(v) MI(#v, -1, v)
#define MI(nm, ix, v)                       \
    do {                                    \
        S32 t_ = (S32)(v);                  \
        mix(&t_, sizeof t_);                \
        if (s_teleN < TELE_MAX) {           \
            s_teleName[s_teleN] = (nm);     \
            s_teleIdx[s_teleN] = (S16)(ix); \
            s_tele[s_teleN++] = t_;         \
        }                                   \
    } while (0)

/* "hero->Obj.X", "var.game[77]", or "obj[12].Anim" for the actor tables, which are the
   one indexed thing a person reads with the index in the middle. */
void Control_StateValueName(S32 k, char *out, size_t n) {
    const char *nm = s_teleName[k];

    if (s_teleIdx[k] < 0) {
        snprintf(out, n, "%s", nm);
        return;
    }
    if (!strncmp(nm, "obj.", 4)) {
        snprintf(out, n, "obj[%d].%s", (int)s_teleIdx[k], nm + 4);
        return;
    }
    snprintf(out, n, "%s[%d]", nm, (int)s_teleIdx[k]);
}

/* The keyframe's fields, named once. Writer, reader and reporter all walk this table,
   so a field cannot be added to one and missed by another, and the name a divergence
   reports is the name the table gives. Ordered as a person would read it: where the
   hero is, what he is doing, where the camera is, and what modal is up. */
static const char *const s_kfNames[] = {
    /* where */
    "cube",
    "cubemode",
    "hero.x",
    "hero.y",
    "hero.z",
    "hero.beta",

    /* doing what */
    "hero.anim",
    "hero.gen_anim",
    "hero.body",
    "hero.move",
    "hero.life",
    "hero.zone",
    "comportement",
    "weapon",

    /* seen how */
    "cam.beta",
    "cam.alpha",
    "cam.addbeta",
    "cam.dist",

    /* and what is up in front of it */
    "cinema",
    "dial.obj",
    "choice",
    "choices",
    "fade",
    "blackpal",
};
#define KF_COUNT ((S32)(sizeof s_kfNames / sizeof s_kfNames[0]))

/* Live values for the table above, in its order. */
void Control_KeyframeFill(S32 *v) {
    T_OBJET *hero = &ListObjet[NUM_PERSO];
    S32 i = 0;

    v[i++] = (S32)NumCube;
    v[i++] = (S32)CubeMode;
    v[i++] = (S32)hero->Obj.X;
    v[i++] = (S32)hero->Obj.Y;
    v[i++] = (S32)hero->Obj.Z;
    v[i++] = (S32)hero->Obj.Beta;
    v[i++] = (S32)hero->Obj.Anim.Num;
    /* The general animation the object loop selected, as against the concrete one
       playing. Every input combo this repo pins is stated in these terms -- which
       blow, which take-off foot, which dodge -- so a divergence that reports only
       the concrete number makes the reader translate. The digest already mixes it;
       this is the keyframe learning to say its name. */
    v[i++] = (S32)hero->GenAnim;
    v[i++] = (S32)hero->Obj.Body.Num;
    v[i++] = (S32)hero->Move;
    v[i++] = (S32)hero->LifePoint;
    v[i++] = (S32)hero->ZoneSce;
    v[i++] = (S32)Comportement;
    v[i++] = (S32)Weapon;
    v[i++] = (S32)BetaCam;
    v[i++] = (S32)AlphaCam;
    v[i++] = (S32)AddBetaCam;
    v[i++] = (S32)VueDistance;
    v[i++] = (S32)CinemaMode;
    v[i++] = (S32)NumObjDial;
    v[i++] = (S32)GameChoice;
    v[i++] = (S32)GameNbChoices;
    v[i++] = (S32)FlagFade;
    v[i++] = (S32)FlagBlackPal;
}

unsigned long long Control_StateDigest(void) {
    T_OBJET *hero = &ListObjet[NUM_PERSO];
    S32 i;
    s_h = 14695981039346656037ULL;
    s_teleN = 0;
    M(Island);
    M(NumCube);
    M(CubeMode);
    M(NbObjets);
    M(NbZones);
    M(hero->Obj.X);
    M(hero->Obj.Y);
    M(hero->Obj.Z);
    M(hero->Obj.Alpha);
    M(hero->Obj.Beta);
    M(hero->Obj.Gamma);
    M(hero->LifePoint);
    M(Comportement);
    M(Weapon);
    M(hero->Obj.Body.Num);
    M(hero->Obj.Anim.Num);
    M(hero->GenAnim);
    M(hero->Obj.LastFrame);
    M(hero->Move);
    M(hero->ZoneSce);
    M(hero->Flags);
    M(hero->WorkFlags);
    M(BetaCam);
    M(AlphaCam);
    M(GammaCam);
    M(AddBetaCam);
    M(VueDistance);
    M(VueOffsetX);
    M(VueOffsetY);
    M(VueOffsetZ);
    M(FollowCamera);
    M(VueCamera);
    M(CameraZone);
    M(CinemaMode);
    M(MagicLevel);
    M(MagicPoint);
    M(NbGoldPieces);
    M(NbLittleKeys);
    for (i = 0; i < NbObjets; i++) {
        T_OBJET *o = &ListObjet[i];
        MI("obj.X", i, o->Obj.X);
        MI("obj.Y", i, o->Obj.Y);
        MI("obj.Z", i, o->Obj.Z);
        MI("obj.Beta", i, o->Obj.Beta);
        MI("obj.Life", i, o->LifePoint);
        MI("obj.Body", i, o->Obj.Body.Num);
        MI("obj.Anim", i, o->Obj.Anim.Num);
        MI("obj.Move", i, o->Move);
        MI("obj.Flags", i, o->Flags);
    }
    /* Modal and menu state. Without these a replay that opens a menu the recording
       never opened stays "matching" until the menu changes something else, which can
       be hundreds of ticks later and in a field that looks unrelated. */
    M(CinemaMode);
    M(NumObjDial);
    M(GameChoice);
    M(GameNbChoices);
    M(FlagFade);
    M(FlagBlackPal);
    for (i = 0; i < MAX_VARS_GAME; i++)
        MI("var.game", i, ListVarGame[i]);
    for (i = 0; i < MAX_VARS_CUBE; i++)
        MI("var.cube", i, ListVarCube[i]);
    return s_h;
}

S32 Control_StateValueCount(void) { return s_teleN; }

S32 Control_StateValue(S32 k) { return (k >= 0 && k < s_teleN) ? s_tele[k] : 0; }

S32 Control_KeyframeCount(void) { return KF_COUNT; }

const char *Control_KeyframeName(S32 k) {
    return (k >= 0 && k < KF_COUNT) ? s_kfNames[k] : "";
}

/* Is there a scene loaded to read? Boot and the menus have no cube and no actors, and
   a caller that snapshots or digests there gets a description of nothing. */
S32 Control_HasLiveScene(void) { return NumCube >= 0 && NbObjets > 0; }

/* The part of a run's description that comes from the engine rather than from whoever
   is describing it: where the game is, and the settings a replay has been shown to
   turn on. `key=value` lines, one per line, appended to whatever the caller already has.

   The settings are empirical rather than mechanical, which is why this is a hand-written
   list and not a walk of the T_SETTING tables. Two sweeps so far, each changing one cfg
   key at a time on the replay side of a recorded session:

     FollowCamera missing                  diverges at tick 0   (it is in the digest)
     FollowCamGroundClearance missing                  tick 2   (the camera rests further out)
     DetailLevel 3 -> 0                                tick 235 (SetDetailLevel turns RainEnable
                                                                 off, and rain draws from the one
                                                                 shared random stream)

   The mouse block came from a third sweep, over a session that orbits rather than walks,
   because the two before it drove the keyboard and could not have moved any of these. A
   recording stores the raw motion the frame will deliver, and the *replaying* run's
   config is what scales it (SOURCES/EXTFUNC.CPP, MouseCameraSensX / MouseCameraDivisor),
   so the same bytes turn the camera by a different amount and the camera is in the digest:

     MouseCamera 1 -> 0                    diverges at tick 61  (the motion is discarded)
     MouseSensitivityX 4 -> 8                          tick 61  (the first gesture is X-only)
     MouseCameraDivisor 4 -> 2                         tick 61
     MouseSensitivityY 4 -> 8                          tick 131 (the first gesture with a Y)
     MouseInvertY 0 -> 1                               tick 131
     FollowCamOrbitGlide 75 -> 20                      tick 101 (where the first gesture ends
                                                                 and the glide takes over)
     MouseCameraDrag 1 -> 0                            tick 61  (on a second session, whose
                                                                 motion holds no button: with
                                                                 one held both settings orbit
                                                                 and nothing can tell them apart)

   Each tick is the gesture that setting shapes, which is the check that these are the
   settings moving the camera rather than a session that diverges anyway.

   The right stick is the same story, on a session that orbits with the stick instead of
   the mouse. Every one of them lands on the gesture for its axis, and none was carried:

     GamepadCameraAnalog 1 -> 0                        tick 61  (the axes are not read)
     GamepadCameraSensX 5 -> 9                         tick 61
     GamepadCamMaxBeta 36 -> 72                        tick 61
     GamepadDeadzone 8000 -> 12000                     tick 61
     GamepadCameraSensY 5 -> 9                         tick 131 (the gesture on Y)
     GamepadCameraInvertY 0 -> 1                       tick 131
     GamepadCamMaxAlpha 10 -> 20                       tick 131

   GamepadDeadzone is the widest of the set and the likeliest to differ between two
   players, being what a stick with drift gets tuned on. It is not only a camera setting:
   SOURCES/JOYSTICK.CPP applies it to the left stick's axis-to-scancode conversion too, so
   it decides what a recorded deflection means for movement as well.

   These are read in SOURCES/INPUT.CPP rather than SOURCES/CONFIG_FILE.CPP, which is worth
   knowing before concluding that a settings sweep has seen the whole config.

   Still out, and unexercised rather than shown harmless: ManualCameraSmoothing read clean
   on both sessions, and reaching it needs an orbit that leaves FollowCamTargetBeta far
   enough from BetaCam to take the stepped branch in SOURCES/FOLLOWCAM.CPP.

   Named by their lba2.cfg keys, so a mismatch names the line the player would edit. */
S32 Control_DescribeState(char *out, size_t n) {
    int wrote = snprintf(out, n,
                         "mode.fixed_timestep=%d\n"
                         "mode.vsync=%d\n"
                         "setup.island=%d\n"
                         "setup.cube=%d\n"
                         "setup.demoslide=%d\n"
                         "data.distrib=%s\n"
                         "settings.FollowCamera=%d\n"
                         "settings.FollowCamGround=%d\n"
                         "settings.FollowCamGroundClearance=%d\n"
                         "settings.FollowCamReengageFrames=%d\n"
                         "settings.FollowCamDriftDivisor=%d\n"
                         "settings.FollowCamManualHoldFrames=%d\n"
                         "settings.FollowCamHoldAngle=%d\n"
                         "settings.FollowCamHDRecompose=%d\n"
                         "settings.FollowCamHDExcessCap=%d\n"
                         "settings.FollowCamHDPitchGain=%d\n"
                         "settings.FollowCamHDDistGain=%d\n"
                         "settings.FollowCamHDLeanGain=%d\n"
                         "settings.FollowCamOrbitGlide=%d\n"
                         "settings.MouseCamera=%d\n"
                         "settings.MouseCameraDrag=%d\n"
                         "settings.MouseSensitivityX=%d\n"
                         "settings.MouseSensitivityY=%d\n"
                         "settings.MouseInvertY=%d\n"
                         "settings.MouseCameraDivisor=%d\n"
                         "settings.GamepadCameraAnalog=%d\n"
                         "settings.GamepadCameraSensX=%d\n"
                         "settings.GamepadCameraSensY=%d\n"
                         "settings.GamepadCameraInvertY=%d\n"
                         "settings.GamepadCamMaxBeta=%d\n"
                         "settings.GamepadCamMaxAlpha=%d\n"
                         "settings.GamepadDeadzone=%d\n"
                         "settings.DetailLevel=%d\n",
                         (int)FixedTimestep,
                         (int)DisplayVSync,
                         (int)Island, (int)NumCube, (int)DemoSlide,
                         Distrib_Name(DistribVersion),
                         (int)FollowCamera,
                         (int)FollowCamGround,
                         (int)FollowCamGroundClearance,
                         (int)FollowCamReengageFrames,
                         (int)FollowCamDriftDivisor,
                         (int)FollowCamManualHoldFrames,
                         (int)FollowCamHoldAngle,
                         (int)FollowCamHDRecompose,
                         (int)FollowCamHDExcessCap,
                         (int)FollowCamHDPitchGain,
                         (int)FollowCamHDDistGain,
                         (int)FollowCamHDLeanGain,
                         (int)FollowCamOrbitGlide,
                         (int)FlagMouseCamera,
                         (int)FlagMouseCameraDrag,
                         (int)MouseCameraSensX,
                         (int)MouseCameraSensY,
                         (int)MouseCameraInvertY,
                         (int)MouseCameraDivisor,
                         (int)GamepadCameraAnalog,
                         (int)GamepadCameraSensX,
                         (int)GamepadCameraSensY,
                         (int)GamepadCameraInvertY,
                         (int)GamepadCamMaxBeta,
                         (int)GamepadCamMaxAlpha,
                         (int)JoystickDeadzone,
                         (int)DetailLevel);
    return wrote > 0 && (size_t)wrote < n;
}

/* ---- snapshots, and the dance the 1997 serialiser requires ------------------ */

/* SaveGame writes "the current game", so writing one anywhere else means briefly
   becoming that game: the save context is the player's name, path and version, and
   there is no parameterised form. Borrow the three, write, put them back. Kept here
   with the rest of the engine handling rather than in whoever wants a snapshot, so the
   borrowing happens once and is restored on the one path.

   Measured, a save restores everything the state digest covers except the position and
   heading of actors that were moving, which are re-derived from their scripts on load. */
/* The save context is the player's name, the path and the format version. Writing a
   save anywhere but "here" means borrowing all three, and so does driving a load, so the
   borrow lives in one place with one saved copy. Not reentrant on purpose: nothing here
   nests, and a stack of saved contexts would hide a leak rather than prevent one. */
static char s_ctxName[MAX_SIZE_PLAYER_NAME + 1];
static char s_ctxPath[ADELINE_MAX_PATH];
static S32 s_ctxVersion = 0;
static S32 s_ctxHeld = 0;

void Control_BorrowSaveContext(void) {
    if (s_ctxHeld)
        return;
    strncpy(s_ctxName, PlayerName, sizeof s_ctxName - 1);
    s_ctxName[sizeof s_ctxName - 1] = 0;
    snprintf(s_ctxPath, sizeof s_ctxPath, "%s", GamePathname);
    s_ctxVersion = NumVersion;
    s_ctxHeld = 1;
}

void Control_ReturnSaveContext(void) {
    if (!s_ctxHeld)
        return;
    strncpy(PlayerName, s_ctxName, MAX_SIZE_PLAYER_NAME);
    PlayerName[MAX_SIZE_PLAYER_NAME] = 0;
    snprintf(GamePathname, ADELINE_MAX_PATH, "%s", s_ctxPath);
    NumVersion = s_ctxVersion;
    s_ctxHeld = 0;
}

S32 Control_WriteSnapshot(const char *path) {
    if (path == NULL || path[0] == 0)
        return 0;

    Control_BorrowSaveContext();
    snprintf(GamePathname, ADELINE_MAX_PATH, "%s", path);
    strncpy(PlayerName, "rec", MAX_SIZE_PLAYER_NAME);
    PlayerName[MAX_SIZE_PLAYER_NAME] = 0;
    NumVersion = NUM_VERSION | SAVE_COMPRESS;
    SaveGame(TRUE);
    Control_ReturnSaveContext();
    return 1;
}

/* MainLoop clears FlagLoadGame at `restartloop`, which it passes through on entry and
   on a restart, not once per iteration. A load driven from outside never returns there,
   so the flag stays set where a fresh --load's would be clear, and several places in the
   object loop branch on it. */
void Control_ClearLoadFlag(void) { FlagLoadGame = FALSE; }

/* Ask the engine to load a save. It lands on MainLoop's own `NewCube != -1` block at
   the top of the next tick, which is why the caller needs Control_LoadSettled to know
   when it has. */
void Control_RequestLoad(const char *path) {
    if (path == NULL || path[0] == 0)
        return;
    snprintf(GamePathname, ADELINE_MAX_PATH, "%s", path);
    FlagLoadGame = TRUE;
    LoadGameNumCube();
}

/* ChangeCube has consumed the pending load. */
S32 Control_LoadSettled(void) { return NewCube == -1; }

S32 Control_SaveVersion(void) { return NumVersion; }

void Control_SetSaveVersion(S32 v) { NumVersion = v; }

#ifdef __cplusplus
}
#endif
