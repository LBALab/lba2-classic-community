#include "CONTROL.H"

#include "C_EXTERN.H" /* pulls DEFINES.H (T_OBJET) + game globals */
#include "COMMON.H"
#include "PERSO.H"  /* InitGame, MainLoop, TheEnd */
#include "OBJECT.H" /* ChangeCube */
#include "PATCH.H"  /* NbPatches */
#include "DIRECTORIES.H"
#include "CONSOLE/CONSOLE.H"

#include <3D/PROJREC.H> /* Projrec_BeginCapture / Projrec_EndCapture */
#include <FILEIO/SAVEPNG.H>
#include <SVGA/SCREEN.H>   /* Screen, ModeDesiredX/Y */
#include <SYSTEM/TIMER.H>  /* TimerRefHR, NbFramePerSecond */
#include <SYSTEM/WINDOW.H> /* Window_SetVSync */
#include <SYSTEM/FILES.H>  /* ExistsFileOrDir */

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

/* --resolution WxH: override the engine's framebuffer dimensions at launch.
   Doesn't imply harness mode — a user can pass this alone for interactive play.
   Default (when not given) is the compile-time RESOLUTION_X/RESOLUTION_Y. */
static int s_have_resolution = 0;
static long s_res_x = 0;
static long s_res_y = 0;
#define CONTROL_RES_MAX_X 1920
#define CONTROL_RES_MAX_Y 1080

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
        if (!strcmp(a, "--resolution") && has_val) {
            /* Format: WxH (or WXH). Width must be a positive multiple of 8 (matches
               the engine's blitter / clip stride assumptions; same constraint as the
               compile-time check in RESOLUTION.H). Height bounded by ADELINE_MAX_Y_RES
               (=1024) via the CONTROL_RES_MAX_Y cap. */
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
                    w > 0 && h > 0 &&
                    w <= CONTROL_RES_MAX_X && h <= CONTROL_RES_MAX_Y &&
                    (w & 7) == 0) {
                    s_have_resolution = 1;
                    s_res_x = w;
                    s_res_y = h;
                    parsed = 1;
                }
            }
            if (!parsed) {
                fprintf(stderr,
                        "[control] --resolution: invalid '%s' (need WxH; W>0 and W%%8==0; "
                        "max %dx%d)\n",
                        val, CONTROL_RES_MAX_X, CONTROL_RES_MAX_Y);
            }
            read += 2;
            continue;
        }

        argv[write++] = argv[read++];
    }
    argv[write] = NULL;
    *argc = write;
}

int Control_IsActive(void) {
    return s_active;
}

int Control_IsVerbose(void) {
    return s_verbose;
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
       of waiting for input. Combined with --fixed-dt this is a deterministic scripted run.
       Opt-in; default gameplay is unaffected. */
    if (s_demo)
        DemoSlide = TRUE;

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
            "\"num_objects\": %d, \"num_zones\": %d, \"num_tracks\": %d, \"num_patches\": %d },\n",
            (int)Island, (int)NumCube, CubeMode == CUBE_INTERIEUR ? "interior" : "exterior",
            (int)NbObjets, (int)NbZones, (int)NbBrickTrack, (int)NbPatches);

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
    SavePNG((char *)path, Screen, (S32)ModeDesiredX, (S32)ModeDesiredY, PtrPal);
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
