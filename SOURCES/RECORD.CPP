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
#include "DISTRIB.H"
#include "INPUT_BINDINGS.H"
#include "LZSS.H"
#include "FOLLOWCAM.H"
#include "JOYSTICK.H"
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
#define REC_VERSION 5

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

void Record_ArmRecord(const char *path) {
    snprintf(s_armRecord, sizeof s_armRecord, "%s", path ? path : "");
    s_armReplay[0] = 0;
    s_armed = 1;
}

static int s_replayFromCli = 0;

void Record_ArmReplay(const char *path) {
    snprintf(s_armReplay, sizeof s_armReplay, "%s", path ? path : "");
    s_armRecord[0] = 0;
    s_armed = 1;
    s_replayFromCli = 1;
}

static void record_flush_at_exit(void) { Record_Stop(); }

int Record_Start(const char *path) {
    char buf[512];
    if (s_recording || s_replaying)
        return 0;
    path = expand_home(path, buf, sizeof buf);
    s_f = fopen(path, "wb");
    if (!s_f) {
        Console_Print("rec: cannot write %s", path);
        return 0;
    }

    char mode[1024];
    /* A player quits through the menu, a crash does not quit at all: flush on any exit
       rather than only on `rec stop`. */
    atexit(record_flush_at_exit);
    write_mode_lines(mode, sizeof mode);
    fprintf(s_f, "%s %d\n%s\n", REC_MAGIC, REC_VERSION, mode);

    memset(s_prevKeys, 0, sizeof s_prevKeys);
    s_prevKey = 0;
    s_prevSrcDelta = 0xFFFFFFFFu;
    s_prevSrc = Timer_ClockSource();
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

int Record_Stop(void) {
    if (!s_f)
        return 0;
    fclose(s_f);
    s_f = NULL;
    s_recording = 0;
    s_replaying = 0;
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
}

/* ---- replay --------------------------------------------------------------- */

static int replay_poll(void) {
    U8 flags;

    if (!get8(&flags)) {
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

int Record_Play(const char *path) {
    char line[256];
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
        const char *k = strstr(s_headerBuf, "clock.timer_ref_hr=");
        unsigned v = 0;
        s_pendingBaseline = 0;
        if (k && sscanf(k + 19, "%u", &v) == 1) {
            s_baseline = (U32)v;
            s_pendingBaseline = 1;
        }
    }

    memset(s_prevKeys, 0, sizeof s_prevKeys);
    s_prevKey = 0;
    s_prevSrcDelta = 0;
    s_replaySrc = Timer_ClockSource();
    s_refDriftMax = 0;
    s_refDriftFirstTick = -1;
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
    s_replaying = 1;
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
    if (s_recording)
        record_poll();
    else if (s_replaying) {
        if (!replay_poll()) {
            /* The engine is mid-session with nothing left to drive it, and whatever it
               was waiting for will never arrive. Say what happened and leave. */
            fprintf(stderr,
                    "[rec] replay ended at poll %ld: %ld ticks checked, first hash mismatch %ld, "
                    "clock drift max %ld ms\n",
                    s_polls, s_ticksChecked, s_hashMismatchTick, s_refDriftMax);
            Record_Stop();
            if (s_replayFromCli)
                exit(s_hashMismatchTick < 0 ? 0 : 2);
        }
        /* Progress, so a long replay is not a silent one. */
        if ((s_polls % 20000) == 0)
            fprintf(stderr, "[rec] ... poll %ld, tick %ld, mismatch %ld\n",
                    s_polls, s_ticks, s_hashMismatchTick);
    }
}

/* Every ManageTime call inside a frame reads the value that frame's input poll
   sampled, while recording as well as while replaying, so the recording and the replay
   agree by construction instead of by the two runs happening to call ManageTime the
   same number of times. Recording therefore quantises game time to one sample per
   input poll. It touches no mode flag, so nothing else in the engine behaves
   differently. */
void Record_ClockHook(U32 *now) {
    s_mtCalls++;
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

    if (!s_recording && !s_replaying)
        return;
    h = state_hash();

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
        char line[256];
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
