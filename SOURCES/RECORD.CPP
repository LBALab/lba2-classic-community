/* PROTOTYPE. Not for merge. See docs/plan/RECORDING_RESEARCH.md.
 *
 * File layout: a text header of key=value lines, a blank line, then a binary record
 * stream. The header is text so `rec info` can report what a recording was made under
 * and diff it against the live run without a parser.
 *
 * Record stream, one flags byte per record:
 *   bit7  this is a tick record: u32 tick, u64 hash
 *   bit0  key table changed:     u8 n, then n * (u16 index, u8 value)
 *   bit1  Key changed:           s32
 *   bit2  analog block present:  s16 rsx, s16 rsy, u32 padFirst, s32 mdx, s32 mdy, s32 click
 *   bit3  clock delta changed:   u32
 * A poll where nothing changed is one byte.
 */

#include "RECORD.H"

#include "BUILD_INFO.h"
#include "C_EXTERN.H"
#include "CONTROL.H"
#include "CONSOLE/CONSOLE.H"
#include "DIRECTORIES.H"
#include "DISTRIB.H"
#include "INPUT_BINDINGS.H"
#include "RECORD_FORMAT.H"
#include "LZSS.H"
#include "FOLLOWCAM.H"
#include "JOYSTICK.H"
#include "SAVEGAME.H"
#include "MESSAGE.H"

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/TIMER.H>

#include <SDL3/SDL_timer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

/* ---- live state ---------------------------------------------------------- */

#define REC_MAGIC "LBA2REC"
#define REC_VERSION 8

/* Sync markers, on the same idea as the socket protocol's <<END>>: a reader that has
 * lost its place needs something it can find again. Without them a stream that goes out
 * of step reads on into whatever the bytes happen to say, and reports a consistency
 * failure at a nonsense tick, which looks like the simulation diverging rather than the
 * reader being lost. One every REC_SYNC_EVERY polls carries the poll and tick the
 * recording was on, so a replay can say where it drifted and pick the stream back up.
 * 13 bytes per marker against a poll that costs about one, so roughly 0.2 bytes a poll. */
#define REC_SYNC 0x60
#define REC_SYNC_MAGIC 0x53594E43u /* "SYNC" */
#define REC_SYNC_EVERY 64

static FILE *s_f = NULL;
static int s_recording = 0;
static int s_replaying = 0;

static U8 s_prevKeys[TABKEYS_NUM_KEYS];
static S32 s_prevKey = 0;
static U32 s_prevSrcDelta = 0xFFFFFFFFu;
static U32 s_prevSrc = 0;
static U32 s_replaySrc = 0;
/* Reported apart from the state hash on purpose: "did the clock reproduce" and "did
   the simulation reproduce" are different questions and conflating them hides which
   one failed. */
static long s_refDriftMax = 0;
static long s_refDriftFirstTick = -1;
static long s_syncDriftPoll = -1;

static long s_polls = 0;
static long s_ticks = 0;
static long s_bytes = 0;
static long s_quietPolls = 0;
static long s_bytesTicks = 0;
static long s_maxPollsInTick = 0;
static long s_pollsThisTick = 0;

/* Replay-side current sample, re-applied by the mouse hook. */
static S16 s_rsx = 0, s_rsy = 0;
static U32 s_padFirst = 0;
static S32 s_mdx = 0, s_mdy = 0, s_click = 0;

/* Replay verification. */
static long s_clockMismatchPoll = -1;
static long s_hashMismatchTick = -1;
static long s_ticksChecked = 0;
static int s_eof = 0;

static char s_headerBuf[2048];

/* Console commands issued during the session. A recording that carries only input
   cannot reproduce a harness-driven fixture: the walkthrough drives with `teleport`,
   which is a command, not a key. */
#define REC_CMD_MAX 512
typedef struct {
    U32 tick;
    char line[96];
} T_RecCmd;
static T_RecCmd s_cmds[REC_CMD_MAX];
static int s_nCmds = 0;
static int s_cmdRead = 0;
static long s_cmdsRecorded = 0;
static long s_cmdsReplayed = 0;
static U32 s_prevMt = 0;
static long s_mtCalls = 0;
static U32 s_frameClock = 0;
static U32 s_baseline = 0;
static char s_snapshot[ADELINE_MAX_PATH] = "-";
static char s_recPath[ADELINE_MAX_PATH] = "";
/* Whether this session reloaded the snapshot before its first recorded or replayed
   frame, and the header line that says so. A mid-session recording does, because a
   save restores actor kinematics by re-deriving them and only a run that took the same
   load path lands in the same state; a from-boot recording does not, and must not be
   made to. The flag is what keeps those two apart on the replay side: reloading a
   from-boot recording would break a path that reproduces exactly as it is. */
static int s_reloaded = 0;
/* The game clock the reload was performed at. The load stamps state from it that the
   state hash does not cover, so the two ends have to take the load at the same reading
   or they part company later, long after the load looks identical. Measured: one step
   apart, and worth an actor's heading 300 ticks on. */
static unsigned s_reloadClock = 0;
/* Frame pacing while recording. A pinned step makes game time advance by a constant
   per frame, and the engine has no frame limiter, so without holding each frame back
   the session runs at whatever rate it renders at: 600 ticks of a 16 ms step took
   3.53 s of wall clock rather than 9.6. Only while recording, and only when the step
   is pinned; a replay wants to go as fast as it can. */
static U32 s_paceAnchor = 0;
static U32 s_paceFrames = 0;
static int s_pendingBaseline = 0;
static int s_frameClockValid = 0;
static int s_hash32 = 0;    /* LBA2_REC_HASH32: store a 32-bit digest */
static int s_hashEvery = 1; /* LBA2_REC_HASH_EVERY: hash one tick in N */

/* ---- the state hash ------------------------------------------------------ */

static unsigned long long s_h;
static void mix(const void *p, unsigned n) {
    const unsigned char *b = (const unsigned char *)p;
    unsigned i;
    for (i = 0; i < n; i++) {
        s_h ^= b[i];
        s_h *= 1099511628211ULL;
    }
}
#define M(v)                 \
    do {                     \
        S32 t_ = (S32)(v);   \
        mix(&t_, sizeof t_); \
    } while (0)

static unsigned long long state_hash(void) {
    T_OBJET *hero = &ListObjet[NUM_PERSO];
    S32 i;
    s_h = 14695981039346656037ULL;
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
        M(o->Obj.X);
        M(o->Obj.Y);
        M(o->Obj.Z);
        M(o->Obj.Beta);
        M(o->LifePoint);
        M(o->Obj.Body.Num);
        M(o->Obj.Anim.Num);
        M(o->Move);
        M(o->Flags);
    }
    for (i = 0; i < MAX_VARS_GAME; i++)
        M(ListVarGame[i]);
    for (i = 0; i < MAX_VARS_CUBE; i++)
        M(ListVarCube[i]);
    return s_h;
}

/* ---- the binding table the recording was resolved through ----------------- */

/* A replay injects the key table, and `GetInput` rebuilds `Input` from that table *and*
   the bindings, so the bindings are half of the function that turns a recording into
   actions rather than context to note beside it: rebinding one key breaks a replay at
   the tick of the first recorded press. The digest below detects that and cannot repair
   it, so the recording carries the table itself and a replay borrows it for its own
   duration, returning the player's own on the way out, on the same borrow-and-return
   the snapshot reload uses for the save context.

   It is also what keeps this file convertible. The frame intent is a pure function of
   the polled state plus the bindings, so a recording that carries them can be
   re-expressed as a stream of actions later, without being recorded again, on the day
   the reverse lookup exists. A recording that does not carry them cannot. */
/* Sized to MAX_INPUT_SLOTS, which is every slot the header carries and every slot
   InitInput and RestoreInput touch. Not to the live tables: DefKeys is declared with a
   spare entry and GamepadKeys is not, so a copy sized off one of them overruns the
   other by exactly one T_DEF_KEY. */
static T_DEF_KEY s_savedDefKeys[MAX_INPUT_SLOTS];
static T_DEF_KEY s_savedGamepadKeys[MAX_INPUT_SLOTS];
static T_DEF_KEY s_recDefKeys[MAX_INPUT_SLOTS];
static T_DEF_KEY s_recGamepadKeys[MAX_INPUT_SLOTS];
static int s_haveRecBindings = 0;
static int s_bindingsInstalled = 0;

static void bindings_install(void) {
    if (!s_haveRecBindings || s_bindingsInstalled)
        return;
    memcpy(s_savedDefKeys, DefKeys, MAX_INPUT_SLOTS * sizeof DefKeys[0]);
    memcpy(s_savedGamepadKeys, GamepadKeys, MAX_INPUT_SLOTS * sizeof GamepadKeys[0]);
    memcpy(DefKeys, s_recDefKeys, MAX_INPUT_SLOTS * sizeof DefKeys[0]);
    memcpy(GamepadKeys, s_recGamepadKeys, MAX_INPUT_SLOTS * sizeof GamepadKeys[0]);
    /* The combined (key, mask) table is what CheckKey scans; changing the two source
       tables without rebuilding it changes nothing. */
    InitInput();
    s_bindingsInstalled = 1;
}

static void bindings_restore(void) {
    if (!s_bindingsInstalled)
        return;
    memcpy(DefKeys, s_savedDefKeys, MAX_INPUT_SLOTS * sizeof DefKeys[0]);
    memcpy(GamepadKeys, s_savedGamepadKeys, MAX_INPUT_SLOTS * sizeof GamepadKeys[0]);
    InitInput();
    s_bindingsInstalled = 0;
}

/* Digest of the binding table, so a recording knows which bindings resolved it. */
static unsigned long long bindings_digest(void) {
    S32 i;
    s_h = 14695981039346656037ULL;
    for (i = 0; i < MAX_INPUT_SLOTS; i++) {
        M(DefKeys[i].Key1);
        M(DefKeys[i].Key2);
        M(GamepadKeys[i].Key1);
        M(GamepadKeys[i].Key2);
    }
    return s_h;
}

/* ---- little-endian scalars ----------------------------------------------- */

static void put8(U8 v) {
    fputc(v, s_f);
    s_bytes++;
}
static void put16(U16 v) {
    put8((U8)(v & 0xFF));
    put8((U8)(v >> 8));
}
static void put32(U32 v) {
    put16((U16)(v & 0xFFFF));
    put16((U16)(v >> 16));
}
static void put64(unsigned long long v) {
    put32((U32)(v & 0xFFFFFFFFu));
    put32((U32)(v >> 32));
}

static int get8(U8 *v) {
    int c = fgetc(s_f);
    if (c == EOF)
        return 0;
    *v = (U8)c;
    return 1;
}
static int get16(U16 *v) {
    U8 a, b;
    if (!get8(&a) || !get8(&b))
        return 0;
    *v = (U16)(a | (b << 8));
    return 1;
}
static int get32(U32 *v) {
    U16 a, b;
    if (!get16(&a) || !get16(&b))
        return 0;
    *v = (U32)a | ((U32)b << 16);
    return 1;
}
static int get64(unsigned long long *v) {
    U32 a, b;
    if (!get32(&a) || !get32(&b))
        return 0;
    *v = (unsigned long long)a | ((unsigned long long)b << 32);
    return 1;
}

/* ---- the mode header ------------------------------------------------------ */

/* The "what am I running under" report. Everything a replay could trip over. */
/* The file part of a path, for the header. The snapshot is written beside the
   recording, so naming it by path would put the recorder's home directory into a file
   meant to be handed to someone else, and the path would be wrong on their machine
   anyway. */
static const char *path_leaf(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    /* Nested rather than one condition: `AND (` reformats to `AND(`, the macro reading
       as a call as soon as a parenthesised group follows it. */
    if (back != NULL) {
        if (slash == NULL OR back > slash)
            slash = back;
    }
    return (slash != NULL) ? slash + 1 : path;
}

/* The other direction, at replay: put the header's name back beside the recording it
   came with. A name that already carries a separator is left alone, which is what lets
   a recording written before the header held a leaf still find its snapshot. */
static void snapshot_beside(const char *recPath, const char *name, char *out, size_t n) {
    const char *leaf;

    if (strchr(name, '/') != NULL OR strchr(name, '\\') != NULL) {
        snprintf(out, n, "%s", name);
        return;
    }

    leaf = path_leaf(recPath);
    if (leaf == recPath) {
        snprintf(out, n, "%s", name); /* the recording is in the working directory too */
        return;
    }
    snprintf(out, n, "%.*s%s", (int)(leaf - recPath), recPath, name);
}

static void write_mode_lines(char *out, size_t n) {
    snprintf(out, n,
             "engine=%s\n"
             "mode.headless=%d\n"
             "mode.fixed_dt=%d\n"
             "mode.fixed_timestep=%d\n"
             "mode.vsync=%d\n"
             "build.flags=%s\n"
             "mode.resolution=%ux%u\n"
             "mode.language=%d\n"
             "setup.island=%d\n"
             "setup.cube=%d\n"
             "setup.demoslide=%d\n"
             "setup.snapshot=%s\n"
             "setup.reloaded=%d\n"
             "setup.reload_clock=%u\n"
             "clock.timer_ref_hr=%u\n"
             "data.master=%s\n"
             "data.distrib=%s\n"
             "bindings.digest=%016llx\n",
             LBA2_VERSION_STRING,
             Control_IsHeadless(),
             (int)Timer_FixedDtValue(),
             (int)FixedTimestep,
             (int)DisplayVSync,
             BUILD_FLAGS_STR,
             (unsigned)ModeDesiredX, (unsigned)ModeDesiredY,
             (int)Language,
             (int)Island, (int)NumCube, (int)DemoSlide,
             path_leaf(s_snapshot), s_reloaded, s_reloadClock,
             (unsigned)TimerRefHR,
             Distrib_MasterName(Distrib_GetMaster()),
             Distrib_Name(DistribVersion),
             bindings_digest());
}

/* ---- record --------------------------------------------------------------- */

static char s_armRecord[512];
static char s_armReplay[512];
static int s_armed = 0;

/* The console tokenizer hands paths through verbatim and fopen has never known what
   `~` means, so a player typing the obvious path gets a failure with no clue in it. */
static const char *expand_home(const char *path, char *buf, size_t n) {
    const char *home;
    if (!path || path[0] != '~' || (path[1] != '/' && path[1] != 0))
        return path;
    home = getenv("HOME");
    if (!home || !home[0])
        return path;
    snprintf(buf, n, "%s%s", home, path + 1);
    return buf;
}

static int s_recordFromCli = 0;

void Record_ArmRecord(const char *path) {
    snprintf(s_armRecord, sizeof s_armRecord, "%s", path ? path : "");
    s_armReplay[0] = 0;
    s_armed = 1;
    /* The flag records from the first poll of a run whose whole setup is on the command
       line, so the setup reproduces by being typed again. Nothing to reload, and a
       reload here would land the recording somewhere the replay's boot never reaches. */
    s_recordFromCli = 1;
}

static int s_replayFromCli = 0;

void Record_ArmReplay(const char *path) {
    snprintf(s_armReplay, sizeof s_armReplay, "%s", path ? path : "");
    s_armRecord[0] = 0;
    s_armed = 1;
    s_replayFromCli = 1;
}

/* A recording that starts mid-session has no setup: replaying it would need the whole
   run before it reproduced first. A savegame beside the recording is that setup, and
   the engine's own serialiser already writes one. Measured, a save restores everything
   the state hash covers except the position and heading of actors that were moving,
   which are re-derived from their scripts on load, so a replay from the snapshot is
   faithful for the hero, the scene, quest state and every static actor, and the hash
   reports where the moving ones part company.

   The player's own save context is put back afterwards, because recording must not
   disturb the run it observes. */
static void record_save_to(const char *out) {
    char savedName[MAX_SIZE_PLAYER_NAME + 1];
    char savedPath[ADELINE_MAX_PATH];
    S32 savedVersion = NumVersion;

    strncpy(savedName, PlayerName, sizeof savedName - 1);
    savedName[sizeof savedName - 1] = 0;
    strncpy(savedPath, GamePathname, sizeof savedPath - 1);
    savedPath[sizeof savedPath - 1] = 0;

    snprintf(GamePathname, ADELINE_MAX_PATH, "%s", out);
    strncpy(PlayerName, "rec", MAX_SIZE_PLAYER_NAME);
    PlayerName[MAX_SIZE_PLAYER_NAME] = 0;
    NumVersion = NUM_VERSION | SAVE_COMPRESS;
    SaveGame(TRUE);

    strncpy(PlayerName, savedName, MAX_SIZE_PLAYER_NAME);
    PlayerName[MAX_SIZE_PLAYER_NAME] = 0;
    snprintf(GamePathname, ADELINE_MAX_PATH, "%s", savedPath);
    NumVersion = savedVersion;
}

static void record_write_snapshot(const char *recPath, char *out, size_t n) {
    snprintf(out, n, "%s.lba", recPath);
    record_save_to(out);
}

static void record_flush_at_exit(void) { Record_Stop(); }

/* ---- the reload, which both ends of a mid-session recording go through ------ */

/* A mid-session load is not the load `--load` performs, and the difference is not
   cosmetic: measured, the same file loaded at boot and loaded mid-session part company
   on the position and heading of every actor that was moving, while the same file
   loaded mid-session at two different ticks parts company on nothing. So the load is a
   reproducible starting point only if both ends take this path, which is why the setup
   belongs to the recording rather than to the command line.
 *
 * Deferred rather than done here. `LoadGameNumCube` only names the cube; the load lands
 * at the next `ChangeCube` in MainLoop's body, and the tick hook is the one place that
 * runs after it. */
static char s_reloadSave[ADELINE_MAX_PATH];
static char s_reloadThen[ADELINE_MAX_PATH];
static char s_reloadSavedPath[ADELINE_MAX_PATH];
static char s_reloadSavedName[MAX_SIZE_PLAYER_NAME + 1];
static S32 s_reloadSavedVersion = 0;
static int s_reloadForPlay = 0; /* 0 = begin recording after it lands, 1 = begin replay */
static int s_reloadState = 0;   /* 1 = asked for, 2 = issued and waiting for the cube */
/* The tick hook sees the load land; the poll hook is where the session begins. Keeping
   that split matters for one measured reason: the header's clock baseline is sampled
   where recording starts and re-applied where replay starts, so the two have to be the
   same point in the frame. Beginning at the hook instead cost 16 ms of clock drift --
   exactly one step -- and a state mismatch at tick 1. */
static int s_beginAtPoll = 0; /* 0 = nothing pending, 1 = record, 2 = replay */
/* Stopped after the load was issued. It cannot be taken back -- the engine is going to
   change cube either way -- and dropping the state machine there would strand
   FlagLoadGame set, which is the one thing about this path that must not be got wrong.
   So the reload still lands and still cleans up; only the session is called off. */
static int s_reloadAbort = 0;

static void record_reload_request(const char *save, const char *then, int forPlay) {
    snprintf(s_reloadSave, sizeof s_reloadSave, "%s", save);
    snprintf(s_reloadThen, sizeof s_reloadThen, "%s", then);
    s_reloadForPlay = forPlay;
    s_reloadState = 1;
}

static void record_reload_issue(void) {
    /* Before the load, because the load is what reads it. A savegame restores the
       fields the hash covers and re-derives the rest, and some of what it re-derives is
       stamped from the game clock; the two ends do not arrive here with the same
       reading, since writing the snapshot costs the recording a frame the replay never
       spends. Same state at tick 0 either way, and one actor's heading apart 300 ticks
       later, which is what this closes. */
    if (s_reloadForPlay)
        SetTimerHR((U32)s_reloadClock);
    else
        s_reloadClock = (unsigned)TimerRefHR;

    strncpy(s_reloadSavedName, PlayerName, sizeof s_reloadSavedName - 1);
    s_reloadSavedName[sizeof s_reloadSavedName - 1] = 0;
    snprintf(s_reloadSavedPath, sizeof s_reloadSavedPath, "%s", GamePathname);
    s_reloadSavedVersion = NumVersion;

    snprintf(GamePathname, ADELINE_MAX_PATH, "%s", s_reloadSave);
    FlagLoadGame = TRUE;
    LoadGameNumCube();
}

static void record_reload_landed(void) {
    /* MainLoop clears FlagLoadGame at `restartloop`, which it passes through on entry
       and on a restart, not once per iteration. A mid-session load never returns there,
       so the flag stays set where a fresh --load's would be clear, and several places in
       the object loop branch on it. Clearing it here is what makes the two match. */
    FlagLoadGame = FALSE;

    /* Deliberately no reseed here. ChangeCube is the engine's only srand and outside
       the demo reel it seeds from TimerRefHR (SOURCES/OBJECT.CPP), so two ends that
       reach the cube change at different readings draw different streams. Pinning the
       clock before the load settles that at its cause: both ends reach the cube change
       at the same TimerRefHR, so the seed follows without being carried. That also holds
       for cube changes later in a session, which a seed named in the header could not
       reach. */

    /* The recorder does not get to change which save the session is on. */
    strncpy(PlayerName, s_reloadSavedName, MAX_SIZE_PLAYER_NAME);
    PlayerName[MAX_SIZE_PLAYER_NAME] = 0;
    snprintf(GamePathname, ADELINE_MAX_PATH, "%s", s_reloadSavedPath);
    NumVersion = s_reloadSavedVersion;
}

/* `haveSnapshot` for the reload path, which wrote and then loaded the snapshot before
   getting here: re-saving it now would name a file the replay never loads, and the
   state a replay has to reach is the one this run reached from the file it took. */
static int record_begin(const char *path, int haveSnapshot) {
    char buf[512];
    if (s_recording || s_replaying)
        return 0;
    path = expand_home(path, buf, sizeof buf);
    s_f = fopen(path, "wb");
    if (!s_f) {
        Console_Print("rec: cannot write %s", path);
        return 0;
    }

    char mode[1400];
    snprintf(s_recPath, sizeof s_recPath, "%s", path);
    if (!haveSnapshot)
        record_write_snapshot(path, s_snapshot, sizeof s_snapshot);
    /* A player quits through the menu, a crash does not quit at all: flush on any exit
       rather than only on `rec stop`. */
    atexit(record_flush_at_exit);
    /* The header's clock baseline has to be the clock the first recorded frame runs on,
       not the one the frame arrived with. A replay applies it through SetTimerHR, whose
       LockTimer banks the frame's pending delta a line before the assignment throws it
       away, so a baseline sampled ahead of that bank leaves the replay one frame of
       clock behind the recording. Arm the frame clock and let ManageTime settle it.
       On the flag path the pending delta is zero, since the first recorded poll is the
       run's first poll, and this changes nothing. After a mid-session reload it is one
       step, and that step was the whole of a 16 ms drift and a state mismatch at
       tick 1. */
    s_frameClock = Timer_ClockSource();
    s_frameClockValid = 1;
    ManageTime();
    write_mode_lines(mode, sizeof mode);
    fprintf(s_f, "%s %d\n%s", REC_MAGIC, REC_VERSION, mode);
    {
        /* Their own lines rather than part of the mode block: they are payload a replay
           installs, not a mode a reader compares, and bindings.digest above is the
           comparison. */
        char tab[512];
        /* A refused write means the table did not fit, and a short one parses as a
           different set of bindings rather than as an error, so drop the line
           instead: a replay without it says so and borrows the live tables. */
        if (RecFmt_WriteBindings(tab, sizeof tab, DefKeys, MAX_INPUT_SLOTS,
                                 "bindings.keyboard="))
            fputs(tab, s_f);
        if (RecFmt_WriteBindings(tab, sizeof tab, GamepadKeys, MAX_INPUT_SLOTS,
                                 "bindings.gamepad="))
            fputs(tab, s_f);
    }
    fputc('\n', s_f);

    memset(s_prevKeys, 0, sizeof s_prevKeys);
    s_prevKey = 0;
    s_prevSrcDelta = 0xFFFFFFFFu;
    s_prevSrc = Timer_ClockSource();
    /* Say so at the moment of recording rather than leaving it to be discovered on
       replay. The step is pinned at the top of Control_Begin, so a run that did not ask
       for it there cannot have it now, and a session simulated on a loose step will not
       replay exactly however faithfully its input was captured. */
    if (!Timer_FixedDtActive())
        Console_Print("rec: warning, the simulation step is not pinned, so this "
                      "recording will not replay exactly. Relaunch with --fixed-dt 16.");

    s_paceAnchor = SDL_GetTicks();
    s_paceFrames = 0;
    s_polls = s_ticks = s_bytes = s_quietPolls = 0;
    s_bytesTicks = s_maxPollsInTick = s_pollsThisTick = 0;
    s_nCmds = s_cmdRead = 0;
    s_cmdsRecorded = 0;
    s_cmdsReplayed = 0;
    s_prevMt = Timer_ClockSource();
    s_mtCalls = 0;
    s_frameClock = 0;
    s_frameClockValid = 0;
    s_hash32 = getenv("LBA2_REC_HASH32") != NULL;
    {
        const char *e = getenv("LBA2_REC_HASH_EVERY");
        s_hashEvery = e ? atoi(e) : 1;
        if (s_hashEvery < 1)
            s_hashEvery = 1;
    }
    s_recording = 1;
    return 1;
}

int Record_Start(const char *path) {
    char buf[512];
    char snap[ADELINE_MAX_PATH];

    if (s_recording || s_replaying || s_reloadState)
        return 0;

    /* From the flag, and from anywhere there is no live scene to snapshot, record where
       we stand. Everything else is a mid-session start, which has no setup unless it
       makes one: write the snapshot, reload it, and let the first recorded frame be the
       first post-load one, so a replay reading this file lands where this run did.
       The marker describes this start, not the run: a session that recorded from the
       flag and then stopped can still start a second, mid-session recording by hand. */
    if (s_recordFromCli || NumCube < 0 || NbObjets <= 0) {
        s_recordFromCli = 0;
        s_reloaded = 0;
        return record_begin(path, 0);
    }

    path = expand_home(path, buf, sizeof buf);
    record_write_snapshot(path, snap, sizeof snap);
    snprintf(s_snapshot, sizeof s_snapshot, "%s", snap);
    record_reload_request(snap, path, 0);
    Console_Print("rec: reloading %s; recording starts from the post-load state", snap);
    return 1;
}

int Record_Stop(void) {
    int wasRecording = s_recording;
    char endPath[ADELINE_MAX_PATH];

    /* Stopped between asking for the reload and the session beginning. Nothing is
       recording yet, and the file for the record half is not even open, so the early
       return below would leave the request armed to start a session nobody wants. */
    if (s_reloadState || s_beginAtPoll) {
        if (s_reloadState == 2)
            s_reloadAbort = 1; /* already issued: let it land, then drop it */
        else
            s_reloadState = 0;
        s_beginAtPoll = 0;
        if (!s_f)
            return 1;
    }

    if (!s_f)
        return 0;
    fclose(s_f);
    s_f = NULL;
    s_recording = 0;
    s_replaying = 0;
    bindings_restore();

    /* A second snapshot where the recording ends, on the same mechanics as the one at
       the start. The per-tick hash says which tick a replay stopped matching; this says
       what it ended up with, in fields rather than in a digest, and it is the thing a
       replay's final state can be diffed against directly.
     *
     * Only while a scene is live, and only for a recording: a run that ended in a crash
     * leaves no end snapshot, which is the correct outcome rather than a gap to paper
     * over. Its absence says the session did not finish, which is exactly what a crash
     * repro wants to record. */
    if (wasRecording && NumCube >= 0 && NbObjets > 0) {
        snprintf(endPath, sizeof endPath, "%s.end.lba", s_recPath);
        record_save_to(endPath);
    }
    return 1;
}

static void record_poll(void) {
    U8 flags = 0;
    U16 changed[TABKEYS_NUM_KEYS];
    U16 nchg = 0;
    U16 i;
    U32 src = Timer_ClockSource();
    U32 delta = src - s_prevSrc;
    s_frameClock = src;
    s_frameClockValid = 1;

    for (i = 0; i < TABKEYS_NUM_KEYS; i++) {
        if (TabKeys[i] != s_prevKeys[i]) {
            changed[nchg++] = i;
        }
    }
    if (nchg)
        flags |= 0x01;
    if (Key != s_prevKey)
        flags |= 0x02;

    S16 rsx = 0, rsy = 0;
    JoystickGetRightStick(&rsx, &rsy);
    U32 padFirst = JoystickFirstPressedScancode();
    if (rsx || rsy || padFirst || MouseXDep || MouseYDep || Click)
        flags |= 0x04;
    if (delta != s_prevSrcDelta)
        flags |= 0x08;

    put8(flags);
    if (flags & 0x01) {
        put8((U8)(nchg > 255 ? 255 : nchg));
        /* The prototype never sees more than 255 changes in one poll; a real format
           would need an escape. Reported rather than silently truncated. */
        if (nchg > 255)
            fprintf(stderr, "[rec] warning: %u key changes in one poll, truncated\n", nchg);
        for (i = 0; i < nchg && i < 255; i++) {
            put16(changed[i]);
            put8(TabKeys[changed[i]]);
        }
    }
    if (flags & 0x02)
        put32((U32)Key);
    if (flags & 0x04) {
        put16((U16)rsx);
        put16((U16)rsy);
        put32(padFirst);
        put32((U32)MouseXDep);
        put32((U32)MouseYDep);
        put32((U32)Click);
    }
    if (flags & 0x08)
        put32(delta);

    if (flags == 0)
        s_quietPolls++;

    memcpy(s_prevKeys, TabKeys, sizeof s_prevKeys);
    s_prevKey = Key;
    s_prevSrcDelta = delta;
    s_prevSrc = src;
    s_polls++;
    s_pollsThisTick++;

    if ((s_polls % REC_SYNC_EVERY) == 0) {
        put8(REC_SYNC);
        put32(REC_SYNC_MAGIC);
        put32((U32)s_polls);
        put32((U32)s_ticks);
    }
}

/* ---- replay --------------------------------------------------------------- */

/* Scan forward for the next sync marker and pick the stream up there. Only reached
   when the reader has already lost its place, so losing whatever lies between is the
   point rather than a cost. */
static int replay_resync(void) {
    U32 magic, poll, tick;
    U8 b;
    while (get8(&b)) {
        if (b != REC_SYNC)
            continue;
        {
            long pos = ftell(s_f);
            if (!get32(&magic))
                return 0;
            if (magic != REC_SYNC_MAGIC) {
                fseek(s_f, pos, SEEK_SET);
                continue;
            }
        }
        if (!get32(&poll) || !get32(&tick))
            return 0;
        fprintf(stderr, "[rec] resynced at the marker for poll %u, tick %u\n",
                (unsigned)poll, (unsigned)tick);
        s_polls = (long)poll;
        s_ticks = (long)tick;
        return 1;
    }
    return 0;
}

/* Step over a sync marker and check what it says. Both readers need this: a marker
   lands between a poll and the tick record that follows it, so whichever reads next
   meets it first. */
static int replay_take_sync(U8 *flags) {
    U32 magic, poll, tick;
    while (*flags == REC_SYNC) {
        if (!get32(&magic) || magic != REC_SYNC_MAGIC)
            return 0;
        if (!get32(&poll) || !get32(&tick))
            return 0;
        if ((long)poll != s_polls || (long)tick != s_ticks) {
            if (s_syncDriftPoll < 0) {
                s_syncDriftPoll = s_polls;
                fprintf(stderr,
                        "[rec] stream drifted: this run is at poll %ld tick %ld, the "
                        "recording was at poll %u tick %u\n",
                        s_polls, s_ticks, (unsigned)poll, (unsigned)tick);
            }
            s_polls = (long)poll;
            s_ticks = (long)tick;
        }
        if (!get8(flags))
            return 0;
    }
    return 1;
}

static int replay_poll(void) {
    U8 flags;

    if (!get8(&flags)) {
        s_eof = 1;
        return 0;
    }
    if (flags > 0x0F && flags != REC_SYNC && flags != 0x40 && !(flags & 0x80)) {
        if (!replay_resync()) {
            s_eof = 1;
            return 0;
        }
        return 1;
    }
    if (!replay_take_sync(&flags)) {
        s_eof = 1;
        return 0;
    }
    if (flags == 0x40) {
        /* A command record where a poll was expected: the stream is out of step, and
           guessing past it produces confident nonsense. Stop. */
        fprintf(stderr, "[rec] stream out of step at poll %ld\n", s_polls);
        s_eof = 1;
        return 0;
    }
    if (flags & 0x80) {
        /* A tick record where a poll was expected means the two streams have
           drifted apart, which is itself the desync signal. */
        U32 t;
        unsigned long long h;
        get32(&t);
        get64(&h);
        return 1;
    }

    if (flags & 0x01) {
        U8 n;
        U16 j;
        if (!get8(&n)) {
            s_eof = 1;
            return 0;
        }
        for (j = 0; j < n; j++) {
            U16 idx;
            U8 val;
            if (!get16(&idx) || !get8(&val)) {
                s_eof = 1;
                return 0;
            }
            if (idx < TABKEYS_NUM_KEYS)
                s_prevKeys[idx] = val;
        }
    }
    if (flags & 0x02) {
        U32 k;
        if (!get32(&k)) {
            s_eof = 1;
            return 0;
        }
        s_prevKey = (S32)k;
    }
    if (flags & 0x04) {
        U16 a, b;
        U32 pf, mx, my, cl;
        if (!get16(&a) || !get16(&b) || !get32(&pf) || !get32(&mx) || !get32(&my) || !get32(&cl)) {
            s_eof = 1;
            return 0;
        }
        s_rsx = (S16)a;
        s_rsy = (S16)b;
        s_padFirst = pf;
        s_mdx = (S32)mx;
        s_mdy = (S32)my;
        s_click = (S32)cl;
    } else {
        s_rsx = s_rsy = 0;
        s_padFirst = 0;
        s_mdx = s_mdy = s_click = 0;
    }
    if (flags & 0x08) {
        U32 d;
        if (!get32(&d)) {
            s_eof = 1;
            return 0;
        }
        s_prevSrcDelta = d;
    }

    /* The clock this frame runs on, from the file. */
    s_replaySrc += s_prevSrcDelta;
    s_frameClock = s_replaySrc;
    s_frameClockValid = 1;
    if (s_pendingBaseline) {
        s_pendingBaseline = 0;
        SetTimerHR(s_baseline);
    }

    /* Inject. This is the whole point: every consumer downstream of the funnel
       reads these and cannot tell they were replayed. */
    memcpy(TabKeys, s_prevKeys, sizeof s_prevKeys);
    Key = s_prevKey;
    Joystick_SetReplayState(s_padFirst, s_rsx, s_rsy);
    MouseXDep = s_mdx;
    MouseYDep = s_mdy;
    Click = s_click;

    s_polls++;
    s_pollsThisTick++;
    return 1;
}

/* The counters, sampled where the replay actually starts rather than where it was
   asked for: a replay that reloads first takes a scene load between the two, and the
   clock it compares against has to be the one it begins on. */
static void replay_begin(void) {
    memset(s_prevKeys, 0, sizeof s_prevKeys);
    s_prevKey = 0;
    s_prevSrcDelta = 0;
    s_replaySrc = Timer_ClockSource();
    s_refDriftMax = 0;
    s_refDriftFirstTick = -1;
    s_syncDriftPoll = -1;
    s_polls = s_ticks = s_ticksChecked = 0;
    s_bytesTicks = s_maxPollsInTick = s_pollsThisTick = 0;
    s_nCmds = s_cmdRead = 0;
    s_cmdsRecorded = 0;
    s_cmdsReplayed = 0;
    s_prevMt = Timer_ClockSource();
    s_mtCalls = 0;
    s_clockMismatchPoll = -1;
    s_hashMismatchTick = -1;
    s_eof = 0;
    bindings_install();
    /* The recorded commands are executed through the console, and a run that never used
       --exec has never registered its table: every replayed command then dispatches to
       nothing, silently, and the replay diverges wherever the recording relied on one.
       The harness does this before its own --exec for the same reason. */
    Console_EnsureRegistered();
    s_replaying = 1;
}

int Record_Play(const char *path) {
    char line[640]; /* a binding table is one line and does not fit in 256 */
    char buf[512];

    if (s_recording || s_replaying)
        return 0;
    path = expand_home(path, buf, sizeof buf);
    s_f = fopen(path, "rb");
    if (!s_f) {
        Console_Print("rec: cannot open %s", path);
        return 0;
    }

    /* Refuse a file this build cannot read. Growing the tick record without bumping
       this turns an old recording into plausible garbage, which reads as a consistency
       failure at tick 0 rather than as the format mismatch it is. */
    {
        int ver = 0;
        char magic[16] = {0};
        if (fscanf(s_f, "%15s %d", magic, &ver) != 2 || strcmp(magic, REC_MAGIC) != 0) {
            Console_Print("rec: %s is not a recording", path);
            fclose(s_f);
            s_f = NULL;
            return 0;
        }
        if (ver != REC_VERSION) {
            Console_Print("rec: %s is format %d, this build reads %d", path, ver, REC_VERSION);
            fclose(s_f);
            s_f = NULL;
            return 0;
        }
        rewind(s_f);
    }

    /* Skip the text header: read to the blank line. */
    s_headerBuf[0] = 0;
    while (fgets(line, sizeof line, s_f)) {
        if (line[0] == '\n' || line[0] == '\r')
            break;
        strncat(s_headerBuf, line, sizeof s_headerBuf - strlen(s_headerBuf) - 1);
    }

    /* Everything before the first poll (boot, the save load) ran on the host clock in
       both runs, so the recorded baseline has to be restored. Not here though:
       SetTimerHR unlocks through ManageTime, which banks a delta, and with the frame
       clock not yet armed that delta comes off the host clock. One millisecond of it
       is enough for actors to reach an animation change a tick apart a few hundred
       ticks later. Applied on the first poll instead, where the frame clock already
       holds the recording's value and the banked delta is zero. */
    {
        U32 v = 0;
        s_pendingBaseline = 0;
        if (RecFmt_ReadU32(s_headerBuf, "clock.timer_ref_hr=", &v)) {
            s_baseline = v;
            s_pendingBaseline = 1;
        }
    }

    s_haveRecBindings =
        RecFmt_ParseBindings(s_headerBuf, "bindings.keyboard=", s_recDefKeys,
                             MAX_INPUT_SLOTS) &&
        RecFmt_ParseBindings(s_headerBuf, "bindings.gamepad=", s_recGamepadKeys,
                             MAX_INPUT_SLOTS);
    if (!s_haveRecBindings)
        Console_Print("rec: %s carries no binding table; replaying through this run's own", path);

    /* The one header line that decides whether this replay reloads. A recording that
       reloaded its own snapshot begins from the post-load state, so a replay reaches
       that state the same way or it is comparing against a run it never made; a
       recording that did not reload begins wherever its command line put it, and
       reloading here would break a path that reproduces exactly as it stands. */
    {
        S32 v = 0;
        if (RecFmt_ReadInt(s_headerBuf, "setup.reloaded=", &v) && v) {
            char named[ADELINE_MAX_PATH];
            char snap[ADELINE_MAX_PATH];
            if (RecFmt_ReadStr(s_headerBuf, "setup.snapshot=", named, sizeof named) &&
                strcmp(named, "-") != 0 &&
                (snapshot_beside(path, named, snap, sizeof snap), 1)) {
                U32 clk = 0;
                if (RecFmt_ReadU32(s_headerBuf, "setup.reload_clock=", &clk))
                    s_reloadClock = clk;
                record_reload_request(snap, "", 1);
                Console_Print("rec: reloading %s; replay starts from the post-load state", snap);
                return 1;
            }
            Console_Print("rec: %s reloaded a snapshot it does not name; replaying without it",
                          path);
        }
    }

    replay_begin();
    return 1;
}

/* ---- hooks ---------------------------------------------------------------- */

void Record_PollHook(void) {
    if (s_armed) {
        s_armed = 0;
        if (s_armRecord[0])
            fprintf(stderr, "[rec] %s %s\n",
                    Record_Start(s_armRecord) ? "recording to" : "cannot record to", s_armRecord);
        else if (s_armReplay[0])
            fprintf(stderr, "[rec] %s %s\n",
                    Record_Play(s_armReplay) ? "replaying" : "cannot open", s_armReplay);
    }
    /* A reload has landed and this is the first poll after it. Beginning here rather
       than at the tick hook that saw it puts the mid-session start on the same point in
       the frame as the flag's, which is where the clock baseline is sampled. */
    if (s_beginAtPoll) {
        int forPlay = (s_beginAtPoll == 2);
        s_beginAtPoll = 0;
        s_reloaded = 1;
        if (forPlay) {
            replay_begin();
        } else if (!record_begin(s_reloadThen, 1)) {
            s_reloaded = 0;
            fprintf(stderr, "[rec] cannot record to %s\n", s_reloadThen);
        }
    }
    if (s_recording)
        record_poll();
    else if (s_replaying) {
        if (!replay_poll()) {
            /* The engine is mid-session with nothing left to drive it, and whatever it
               was waiting for will never arrive. Say what happened and leave. */
            fprintf(stderr,
                    "[rec] replay ended at poll %ld: %ld ticks checked, first hash mismatch %ld, "
                    "clock drift max %ld ms, stream drift first at poll %ld\n",
                    s_polls, s_ticksChecked, s_hashMismatchTick, s_refDriftMax,
                    s_syncDriftPoll);
            Record_Stop();
            /* Only when nothing else is driving the run. With the harness armed it has
               a tick budget and artifacts to write, and exiting from here would skip
               the --dump-state the caller asked for. */
            if (s_replayFromCli && !Control_IsActive())
                exit(s_hashMismatchTick < 0 ? 0 : 2);
        }
        /* Progress, so a long replay is not a silent one. */
        if ((s_polls % 20000) == 0)
            fprintf(stderr, "[rec] ... poll %ld, tick %ld, mismatch %ld\n",
                    s_polls, s_ticks, s_hashMismatchTick);
    }
}

/* Quantise game time to one sample per input poll, so every ManageTime call inside a
   frame reads what that frame's poll sampled and the two ends agree by construction
   rather than by happening to call ManageTime the same number of times. It touches no
   mode flag, so nothing else in the engine behaves differently.

   Not under a generated clock, and that exception is the whole of it. `--fixed-dt`
   already produces one value per tick, and the engine steps it deliberately in the
   places that wait without polling: `FadeToPalAndSamples`
   (SOURCES/AMBIANCE.CPP) loops on `Timer_FixedDtPump` plus `ManageTime` until the
   fade reaches FADE_DELAY, and never polls input. Holding the clock at the last poll
   there means the fade never completes and the loop never ends, which is why a
   recording could not cross a scene load and why recording from boot hung on the first
   fade after the menus. Under a clock sampled from the host there is no such pump and
   the quantisation is what makes a replay possible at all, so it stays. */
void Record_ClockHook(U32 *now) {
    s_mtCalls++;
    if (Timer_FixedDtActive())
        return;
    if (s_frameClockValid)
        *now = s_frameClock;
}

void Record_MouseHook(void) {
    /* ManageMouse drains the event accumulator into MouseXDep/MouseYDep, which would
       zero what replay_poll injected. Re-apply. */
    if (s_replaying) {
        MouseXDep = s_mdx;
        MouseYDep = s_mdy;
    }
}

void Record_TickHook(void) {
    unsigned long long h;

    /* Ahead of everything else, including the idle check: while a reload is in flight
       the session is neither recording nor replaying, and this hook is what carries it
       through. Issue the load on one tick, let MainLoop's own `NewCube != -1` block run
       ChangeCube at the top of the next, then begin on the tick after that -- so the
       first recorded or replayed frame is the first post-load one at both ends. */
    if (s_reloadState) {
        if (s_reloadState == 1) {
            record_reload_issue();
            s_reloadState = 2;
            return;
        }
        if (NewCube != -1)
            return; /* ChangeCube has not consumed it yet */
        record_reload_landed();
        s_reloadState = 0;
        if (s_reloadAbort)
            s_reloadAbort = 0;
        else
            s_beginAtPoll = s_reloadForPlay ? 2 : 1;
        return;
    }

    if (!s_recording && !s_replaying)
        return;
    h = state_hash();

    if (s_recording && Timer_FixedDtActive()) {
        U32 step = (U32)Timer_FixedDtValue();
        U32 now, target;
        s_paceFrames++;
        target = s_paceAnchor + s_paceFrames * step;
        now = SDL_GetTicks();
        if (now < target)
            SDL_Delay(target - now);
        else if (now - target > 1000) {
            /* A long stall gives up the time rather than sprinting to win it back. */
            s_paceAnchor = now;
            s_paceFrames = 0;
        }
    }

    if (s_recording) {
        long before = s_bytes;
        if (s_hashEvery > 1 && (s_ticks % s_hashEvery) != 0) {
            s_ticks++;
            s_pollsThisTick = 0;
            return;
        }
        put8(s_hash32 ? 0xC0 : 0x80);
        put32((U32)s_ticks);
        put32(TimerRefHR);
        if (s_hash32)
            put32((U32)(h ^ (h >> 32)));
        else
            put64(h);
        s_bytesTicks += s_bytes - before;
        /* Flush every tick. A recording is most wanted when the run ended badly, and a
           crash runs no atexit handler. One tick of loss beats a whole session of it. */
        fflush(s_f);
        if (s_pollsThisTick > s_maxPollsInTick)
            s_maxPollsInTick = s_pollsThisTick;
        s_pollsThisTick = 0;
        s_ticks++;
        return;
    }

    /* Replay: the next record should be this tick's hash. */
    {
        long pos = ftell(s_f);
        U8 flags;
        U32 t;
        unsigned long long rh;
        if (!get8(&flags))
            return;
        if (!replay_take_sync(&flags))
            return;
        if (!(flags & 0x80)) {
            fseek(s_f, pos, SEEK_SET);
            return;
        }
        if (!get32(&t))
            return;
        {
            U32 rref;
            if (!get32(&rref))
                return;
            {
                long drift = (long)TimerRefHR - (long)rref;
                if (drift < 0)
                    drift = -drift;
                if (drift > s_refDriftMax)
                    s_refDriftMax = drift;
                if (drift != 0 && s_refDriftFirstTick < 0)
                    s_refDriftFirstTick = (long)t;
            }
        }
        if (flags == 0xC0) {
            U32 r32;
            if (!get32(&r32))
                return;
            rh = r32;
            h = h ^ (h >> 32);
            h &= 0xFFFFFFFFu;
        } else if (!get64(&rh)) {
            return;
        }
        s_ticksChecked++;
        if (s_pollsThisTick > s_maxPollsInTick)
            s_maxPollsInTick = s_pollsThisTick;
        s_pollsThisTick = 0;
        if (s_hashMismatchTick < 0 && rh != h) {
            s_hashMismatchTick = (long)t;
            fprintf(stderr, "[rec] consistency failure at tick %u: recorded %016llx, replayed %016llx\n",
                    (unsigned)t, rh, h);
        }
        s_ticks++;

        /* Commands run from here, not from the poll hook: executing one inside the
           keyboard path re-enters the stream reader. A command issued while no tick is
           advancing, which is all of menu time, therefore has nowhere to land. */
        for (;;) {
            U32 ct;
            U16 n, j;
            char line[96];
            U8 f2;
            long p2 = ftell(s_f);
            if (!get8(&f2))
                break;
            if (f2 != 0x40) {
                fseek(s_f, p2, SEEK_SET);
                break;
            }
            if (!get32(&ct) || !get16(&n))
                break;
            for (j = 0; j < n && j + 1 < sizeof line; j++) {
                U8 c;
                if (!get8(&c))
                    break;
                line[j] = (char)c;
            }
            line[j] = 0;
            s_cmdsReplayed++;
            Console_Execute(line);
        }
    }
}

/* Console_Execute: capture what the session was driven with, alongside what it was
   played with. `rec` itself is excluded so a replay does not re-enter the recorder. */
void Record_CommandHook(const char *line) {
    if (!s_recording || !line || !line[0])
        return;
    if (!strncmp(line, "rec", 3) && (line[3] == 0 || line[3] == ' '))
        return;
    {
        /* Written where it happens, so the stream order is the execution order:
           the tick record for N, then every command issued during N. */
        U16 n = (U16)strlen(line);
        U16 j;
        if (n > sizeof s_cmds[0].line - 1)
            n = (U16)(sizeof s_cmds[0].line - 1);
        put8(0x40);
        put32((U32)(s_ticks > 0 ? s_ticks - 1 : 0));
        put16(n);
        for (j = 0; j < n; j++)
            put8((U8)line[j]);
        s_cmdsRecorded++;
    }
}

int Record_IsReplaying(void) { return s_replaying; }
int Record_IsRecording(void) { return s_recording; }

/* ---- compression ---------------------------------------------------------- */

/* The engine already ships LZSS (SOURCES/LZSS.CPP), used for savegames and the HQR
   containers. Measures what it would give on a recording's payload rather than
   guessing, and leaves the file alone. */
unsigned long Record_Squeeze(const char *path) {
    char pbuf[512];
    FILE *f;
    path = expand_home(path, pbuf, sizeof pbuf);
    f = fopen(path, "rb");
    long total, hdr = 0, payload;
    char *in, *out;
    unsigned long packed;
    int c, prev = 0;

    if (!f)
        return 0;
    /* Find the blank line that ends the text header. */
    while ((c = fgetc(f)) != EOF) {
        hdr++;
        if (c == '\n' && prev == '\n')
            break;
        prev = c;
    }
    fseek(f, 0, SEEK_END);
    total = ftell(f);
    payload = total - hdr;
    if (payload <= 0) {
        fclose(f);
        return 0;
    }
    in = (char *)malloc((size_t)payload);
    out = (char *)malloc((size_t)payload * 2 + 64);
    if (!in || !out) {
        free(in);
        free(out);
        fclose(f);
        return 0;
    }
    fseek(f, hdr, SEEK_SET);
    if (fread(in, 1, (size_t)payload, f) != (size_t)payload) {
        free(in);
        free(out);
        fclose(f);
        return 0;
    }
    fclose(f);
    packed = Compress_LZSS(in, out, (unsigned long)payload);
    Console_Print("rec: %s header %ld, payload %ld, LZSS %lu (%.1f%% of payload)",
                  path, hdr, payload, packed, 100.0 * (double)packed / (double)payload);
    free(in);
    free(out);
    return packed;
}

/* ---- reporting ------------------------------------------------------------ */

void Record_Report(const char *path) {
    char live[1024];
    write_mode_lines(live, sizeof live);

    if (!path || !path[0]) {
        Console_Print("rec: %s", s_recording ? "recording" : (s_replaying ? "replaying" : "idle"));
        if (s_recording || s_replaying)
            Console_Print("rec: %ld polls, %ld ticks, %ld quiet polls, %ld bytes",
                          s_polls, s_ticks, s_quietPolls, s_bytes);
        Console_Print("rec: bytes input %ld, bytes hash %ld, max polls in one tick %ld",
                      s_bytes - s_bytesTicks, s_bytesTicks, s_maxPollsInTick);
        Console_Print("rec: console commands recorded %ld, replayed %ld, ManageTime samples %ld",
                      s_cmdsRecorded, s_cmdsReplayed, s_mtCalls);
        if (s_replaying) {
            Console_Print("rec: ticks checked %ld, first hash mismatch %ld",
                          s_ticksChecked, s_hashMismatchTick);
            Console_Print("rec: game-clock drift max %ld ms, first at tick %ld",
                          s_refDriftMax, s_refDriftFirstTick);
        }
        Console_Print("rec: live mode --");
        {
            char *p = live, *nl;
            while ((nl = strchr(p, '\n')) != NULL) {
                *nl = 0;
                Console_Print("  %s", p);
                p = nl + 1;
            }
        }
        return;
    }

    /* Read the header of a recording and name every line that differs from this run. */
    {
        FILE *f = fopen(path, "rb");
        char line[640];
        char hdr[1024];
        int diffs = 0;
        if (!f) {
            Console_Print("rec: cannot open %s", path);
            return;
        }
        hdr[0] = 0;
        fgets(line, sizeof line, f); /* magic */
        Console_Print("rec: %s header --", path);
        while (fgets(line, sizeof line, f)) {
            char *nl;
            if (line[0] == '\n' || line[0] == '\r')
                break;
            nl = strchr(line, '\n');
            if (nl)
                *nl = 0;
            if (!strncmp(line, "bindings.keyboard=", 18) || !strncmp(line, "bindings.gamepad=", 17)) {
                /* Payload rather than mode: a few hundred characters of table that
                   bindings.digest above already answers for. */
                continue;
            }
            Console_Print("  %s", line);
            strncat(hdr, line, sizeof hdr - strlen(hdr) - 1);
            strncat(hdr, "\n", sizeof hdr - strlen(hdr) - 1);
        }
        fclose(f);

        /* Diff line by line against the live run. */
        {
            char *p = hdr, *nl;
            while ((nl = strchr(p, '\n')) != NULL) {
                *nl = 0;
                if (p[0] && !strstr(live, p)) {
                    Console_Print("rec: MISMATCH %s", p);
                    diffs++;
                }
                p = nl + 1;
            }
        }
        if (!diffs)
            Console_Print("rec: live run matches the recording's mode");
        else
            Console_Print("rec: %d mode line(s) differ; replay may not reproduce", diffs);
    }
}

} /* extern "C" */
