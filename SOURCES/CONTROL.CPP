#include "CONTROL.H"

#include "C_EXTERN.H" /* pulls DEFINES.H (T_OBJET) + game globals */
#include "COMMON.H"
#include "PERSO.H"    /* InitGame, MainLoop, TheEnd */
#include "OBJECT.H"   /* ChangeCube */
#include "GAMEMENU.H" /* BeginDemoSlide */
#include "PATCH.H"    /* NbPatches */
#include "DIRECTORIES.H"
#include "RES_CATALOG.H" /* RES_MIN_W / RES_MAX_W / RES_MIN_H / RES_MAX_H bounds */
#include "RES_SWITCH.H"  /* Res_Switch for --res-switch-test */
#include "CONSOLE/CONSOLE.H"

#include <3D/PROJREC.H> /* Projrec_BeginCapture / Projrec_EndCapture */
#include <FILEIO/SAVEPNG.H>
#include <SVGA/SCREEN.H>   /* Screen, ModeDesiredX/Y */
#include <SYSTEM/TIMER.H>  /* TimerRefHR, NbFramePerSecond */
#include <SYSTEM/WINDOW.H> /* Window_SetVSync */

#include <SDL3/SDL.h>     /* SDL_SetHint for --headless (must precede SDL_Init) */
#include <SYSTEM/FILES.H> /* ExistsFileOrDir */
#include <SYSTEM/LOG.H>   /* LogSeverity, Log_ParseLevel for --log-level */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Config (parsed once) --------------------------------------------------- */
#define CONTROL_EXEC_MAX 1024

static int s_active = 0;
static int s_armed = 0;

static const char *s_load_name = NULL; /* --load value (NULL = none) */
static int s_have_exec = 0;
static char s_exec[CONTROL_EXEC_MAX];
static int s_have_tick = 0;
static long s_tick = 0;
static int s_have_fixed_dt = 0;
static long s_fixed_dt = 0;
static int s_have_fixed_timestep = 0;
static long s_fixed_timestep = 0;
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
               DE / SP / IT / PO).  strcasecmp is byte-wise ASCII case-folding, so
               the cedilla / tilde bytes in Français / Español compare exactly —
               case-insensitive on the ASCII letters only, which is the same
               behaviour InitLanguage() in MESSAGE.CPP gets from the cfg parse. */
            const char *val = argv[read + 1];
            int matched = -1;
            for (int n = 0; n < CONTROL_LANG_COUNT; n++) {
                if (!strcasecmp(val, kControlLanguageFull[n]) || !strcasecmp(val, kControlLanguageShort[n])) {
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
        if (!strcmp(a, "--headless")) {
            /* No window and no audio device: the supported way to run automation.
               A visible window pauses the game on focus loss (HandleEventsTimer) and a
               live audio thread branches the sim, so a windowed run is only reproducible
               as long as nothing else touches the desktop. Six identical windowed runs in
               parallel produced five different sim states; headless they are identical.

               The hints must be set before SDL_Init, which is why this is parsed here and
               not later: Control_ParseArgs runs first thing in main(). */
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
            SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
            s_headless = 1;
            s_no_audio = 1;
            s_active = 1;
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
       window, so never let focus loss stop its clock. */
    if (s_headless || s_have_tick || s_exit) {
        Timer_SetIgnoreFocus(1);
    }

    if (!s_headless && (s_have_tick || s_exit)) {
        fprintf(stderr,
                "[control] warning: batch run without --headless. The window still opens, "
                "and a live audio thread branches the simulation, so results (screenshots, "
                "--dump-state) may not reproduce. Pass --headless.\n");
    }
}

int Control_IsActive(void) {
    return s_active;
}

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

int Control_NoAudio(void) {
    return s_no_audio;
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
       ChangeCube's srand through Demo_RngSeed (SOURCES/OBJECT.CPP), which seeds from the
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
            fflush(stderr);
            return 0;
        }
        /* Restore exactly as the menu's load_game path does: ChangeCube must run
           while FlagLoadGame is still set (InitGame(-1) sets it; MainLoop clears it
           at its top), otherwise the saved hero position is not restored. */
        InitGame(-1);
        ChangeCube();
        FlagFade = FALSE;
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

/* --- Tick driver ------------------------------------------------------------ */
static long control_effective_ticks(void) {
    long n = s_have_tick ? s_tick : 0;
    if ((s_have_shot || s_have_polyrec) && n < 1)
        n = 1; /* a rendered artifact needs at least one completed AffScene */
    return n;
}

static void control_run_exec(void) {
    char buf[CONTROL_EXEC_MAX];
    char *p, *seg;

    if (!s_have_exec)
        return;

    /* Mirror console output to stdout so --exec results are observable / testable. */
    Console_SetLineSinkForTests(control_console_echo);
    Console_EnsureRegistered();

    copy_arg(buf, sizeof(buf), s_exec);
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
                    "\"life\": %d, \"body\": %d, \"anim\": %d, \"move\": %d, \"flags\": %u }",
                    i ? "," : "", (int)i,
                    (int)o->Obj.X, (int)o->Obj.Y, (int)o->Obj.Z, (int)o->Obj.Beta,
                    (int)o->LifePoint, (int)o->Obj.Body.Num, (int)o->Obj.Anim.Num,
                    (int)o->Move, (unsigned int)o->Flags);
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

    if (s_exit) {
        fflush(stdout);
        TheEnd(PROGRAM_OK, "");
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

void Control_TickHook(void) {
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
    if ((s_hook_calls - 1) >= control_effective_ticks())
        control_finalize();
}

#ifdef __cplusplus
}
#endif
