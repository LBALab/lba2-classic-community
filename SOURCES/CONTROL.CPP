#include "CONTROL.H"

#include "C_EXTERN.H" /* pulls DEFINES.H (T_OBJET) + game globals */
#include "COMMON.H"
#include "PERSO.H"  /* InitGame, MainLoop, TheEnd */
#include "OBJECT.H" /* ChangeCube */
#include "PATCH.H"  /* NbPatches */
#include "DIRECTORIES.H"
#include "CONSOLE/CONSOLE.H"

#include <FILEIO/SAVEPNG.H>
#include <SVGA/SCREEN.H>  /* Screen, ModeDesiredX/Y */
#include <SYSTEM/TIMER.H> /* TimerRefHR, NbFramePerSecond */
#include <SYSTEM/FILES.H> /* ExistsFileOrDir */

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
static int s_have_dump = 0;
static char s_dump_path[ADELINE_MAX_PATH];
static int s_have_shot = 0;
static char s_shot_path[ADELINE_MAX_PATH];
static int s_exit = 0;

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
            s_tick = strtol(argv[read + 1], NULL, 0);
            if (s_tick < 0)
                s_tick = 0;
            s_have_tick = 1;
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
        if (!strcmp(a, "--exit")) {
            s_exit = 1;
            s_active = 1;
            read += 1;
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

void Control_Begin(void) {
    if (s_load_name) {
        if (!control_resolve_save(s_load_name)) {
            fprintf(stderr, "[control] load: save not found: %s\n", s_load_name);
            fflush(stderr);
            TheEnd(PROGRAM_OK, "");
            return;
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

    s_hook_calls = 0;
    s_finalized = 0;
    s_armed = 1;
}

/* --- Tick driver ------------------------------------------------------------ */
static long control_effective_ticks(void) {
    long n = s_have_tick ? s_tick : 0;
    if (s_have_shot && n < 1)
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

static void control_finalize(void) {
    if (s_finalized)
        return;
    s_finalized = 1;

    if (s_have_dump)
        control_dump_state(s_dump_path);
    if (s_have_shot)
        control_screenshot(s_shot_path);

    if (s_exit) {
        fflush(stdout);
        TheEnd(PROGRAM_OK, "");
    }
    /* Not exiting: disarm and hand control back to the running game. */
    s_armed = 0;
}

void Control_TickHook(void) {
    if (!s_armed)
        return;
    s_hook_calls++;
    if (s_hook_calls == 1)
        control_run_exec();
    if ((s_hook_calls - 1) >= control_effective_ticks())
        control_finalize();
}

#ifdef __cplusplus
}
#endif
