/* Session recording. Usage and limits in docs/RECORDING.md; the design work and what
 * it measured, in docs/plan/RECORDING_RESEARCH.md.
 *
 * File layout: a text header of key=value lines, a blank line, the savegame the session
 * started from, then a binary record stream, and at the end the savegame it finished
 * at. The header is text so `rec info` can report what a recording was made under and
 * diff it against the live run without a parser.
 *
 * One file, because a stream with two savegames beside it is a recording only while
 * nothing separates them, and nothing keeps a set of files together: copy or move one
 * and a replay silently starts from somewhere else. The snapshots are 4 to 17 KB
 * against a stream of megabytes, so carrying them costs nothing worth that. Their frame
 * is in RECORD_FORMAT.H, and it is shaped so a run that dies mid-write leaves a file
 * that still reads up to the last thing it finished writing.
 *
 * Record stream, one flags byte per record:
 *   bit7  this is a tick record: u32 tick, u64 hash
 *   bit0  key table changed:     u8 n, then n * (u16 index, u8 value)
 *   bit1  Key changed:           s32
 *   bit2  analog block present:  s16 rsx, s16 rsy, u32 padFirst, s32 mdx, s32 mdy, s32 click
 *   bit3  clock delta changed:   u32
 * A poll where nothing changed is one byte.
 *
 * Two record types stand outside that encoding. 0x60 is a sync marker (see below), and
 * 0x50 is a keyframe: u32 tick, u16 count, then count * s32.
 *
 * The count is what keeps that record readable across builds, and it is not decoration.
 * A reader that sizes a keyframe by asking the *live* build how many fields it names is
 * reading one number out of the file and another out of itself, so the day the field
 * list grows, every recording written before it reads past the end of its first keyframe
 * and takes the rest of the stream from the wrong place. That surfaces as a consistency
 * failure at an enormous tick with hundreds of thousands of seconds of clock drift: the
 * plausible garbage the version check exists to prevent, arriving with no version change
 * to catch it. Nothing else in the stream may be sized this way either.
 *
 * The keyframe exists because the per-tick hash says *when* a replay stopped matching
 * and nothing about *what*. For a fixture that is enough, since the run can be recorded
 * again and the two dumps diffed; for a session a person played it is not, because
 * there is no second run to diff against. So a recording carries a little named state
 * of its own, and a divergence can name the field that moved.
 */

#include "RECORD.H"

#include "BUILD_INFO.h"
#include "CONTROL.H"
#include "CONSOLE/CONSOLE.H"
#include "DIRECTORIES.H"
#include "DISTRIB.H"
#include "INPUT_BINDINGS.H"
#include "RECORD_FORMAT.H"

/* Narrow on purpose. This module reads no game state: it asks CONTROL.H for the digest,
   the keyframe and the state description, so it does not pull C_EXTERN.H and with it
   the DEFINES.H aggregation (scripts/ci/check-build-graph.py rule 2). */
#include <SVGA/SCREEN.H>   /* ModeDesiredX / ModeDesiredY for the mode header */
#include <SYSTEM/FILES.H>  /* ExistsFileOrDir, for resolving a bare recording name */
#include <SYSTEM/LIMITS.H> /* ADELINE_MAX_PATH */
#include "LZSS.H"
#include "JOYSTICK.H"
#include "SAVEGAME.H"
#include "MESSAGE.H"

#include <SYSTEM/KEYBOARD.H>
#include <SYSTEM/MOUSE.H>
#include <SYSTEM/PLATFORM.H> /* Sys_PlatformName, so this file carries no #ifdef chain */
#include <SYSTEM/RANDOM.H>   /* Rnd_StreamName for the numeric contract in the header */
#include <SYSTEM/STRING.H>   /* ade_strcasestr, for the .rec suffix test */
#include <SYSTEM/TIMER.H>

#include <SDL3/SDL_timer.h>

#include <dirent.h> /* the recordings folder, for `rec play` with no argument */
#include <float.h>  /* LDBL_MANT_DIG, one half of the numeric contract */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h> /* the stamp in an auto-named recording */

extern "C" {

/* ---- live state ---------------------------------------------------------- */

#define REC_MAGIC "LBA2REC"
#define REC_VERSION 12

/* The oldest format this build still reads. Every version since has only added record
   types the reader probes for and steps over, so an older recording reads the same as
   it always did -- and a recording of a session someone played once is worth more than
   the tidiness of a single supported version. A change that alters what an existing
   record *means* moves this up to REC_VERSION; a change that only adds one does not.

   A change to what the engine *computes* is a third case, and it does not belong here
   either way: the format still means what it meant, and whether an old recording still
   reproduces is a question about the engine, not about the reader. Moving Rnd off libc
   is the worked example. It changed every draw on Windows and none on Linux, because
   the replacement reproduces glibc, so the recordings that exist still replay and there
   was nothing for a version gate to protect. The header's `numeric.` lines are what
   report that class of difference, per recording, rather than a version refusing the
   whole file.

   12 changes a meaning and still leaves the floor at 9, which is the case the rule does
   not cover: a reader that can tell the versions apart can read both. The analog block's
   mouse pair went from the motion the last ManageMouse delivered to the motion the next
   one will, a frame apart, and nothing in the bytes tells the two readings apart. Only a
   session recorded with a mouse camera reads differently, and no such session can have
   replayed, because both ends of an earlier pair read the stale value and diverge on the
   tick the orbit starts. Every recording that replays clean is keyboard-only, where the
   pair is 0 under either reading and the file is bit-identical. Refusing them would give
   up sessions that work to protect sessions that never did. */
#define REC_VERSION_MIN 9

/* The header declares CONTROL_DIGEST_VERSION (SOURCES/CONTROL.H), which is the set of
   fields Control_StateDigest hashes, and it is deliberately not REC_VERSION: the rule
   above puts a change to what the engine computes in a third case the format version does
   not cover, because the bytes still mean what they meant and the reader can still read
   them. What such a change does break is the comparison, and silently -- a recording whose
   digests were computed over fewer fields mismatches on tick 0 and reads as a session that
   diverged immediately. Declaring it puts that in the mode diff beside the `numeric.`
   lines, where a replay names what it disagrees about instead of failing as though the run
   came apart. */

/* Every buffer the text header passes through, named once so growing it is one decision
   rather than five. A real header runs about 1.6 KB: the mode block, the settings, and
   two binding tables that scale with how much the player has rebound. It grows whenever a
   setting is shown to move a replay, and the failure at the far end is quiet in both
   directions -- the writer drops the lines at the end of it, and the reader truncates
   mid-binding-table and installs a partial one -- so the margin is deliberate rather than
   a round number that happened to fit. */
#define REC_HEADER_MAX 4096

/* Versions 9 to 11 wrote the keyframe with no count and exactly this many fields. */
#define REC_KF_LEGACY_FIELDS 23

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

/* Keyframe: named state beside the digest, so a mismatch can say which field moved.
   Every 32 ticks rather than every tick because it is diagnosis rather than detection
   -- the hash already catches the tick -- and one every 32 costs about 8 KB over a
   3700-tick session. */
#define REC_KEY 0x50
#define REC_KEY_EVERY 32
/* Upper bound for the stack keyframe buffers. The harness owns the real count, so this
   only has to be no smaller -- and record_keyframe_fit() checks that rather than
   trusting it, because the failure would be a silent write past a stack array. */
#define REC_KF_MAX 64

/* Verbose telemetry: every value the digest mixes, stored raw beside it.
 *
 * The digest is lossy by construction -- it answers "did this tick match" and nothing
 * else -- so a divergence in a session nobody can re-record leaves ~1300 candidate
 * fields and no way to choose between them. The keyframe narrowed that to 23 named
 * fields every 32 ticks, which covers the hero and the open modal and misses the other
 * actors and all 336 script variables entirely. This carries the lot, every tick, so
 * the first mismatching tick names the field that moved.
 *
 * About 2 KB a tick, so it is opt-in (--verbose) rather than the default: a diagnostic
 * recording, made once, to answer a question a normal one cannot. The bytes are the
 * cost and the time is not -- measured, 0.10 ms of CPU a tick, which is the loop that
 * marshals the values rather than the write.
 */
#define REC_TELE 0x51

/* The savegames the file carries: the state the session started from, written once
 * between the header and the first record, and the state it finished at, appended when
 * it stops. Framed by RECORD_FORMAT.H, which is also where the reasoning about a torn
 * write lives.
 *
 * The end chunk is a trailer, and its absence is load-bearing rather than untidy:
 * Record_Stop only writes it for a session that stopped while a scene was live, so a
 * run that died leaves a recording with no end state, and that is what says the session
 * did not finish. A crash repro wants exactly that recorded.
 *
 * A recording in an older format carries neither, and names its snapshot beside itself
 * in `setup.snapshot=` instead; both readers below take that path too, which is why
 * REC_VERSION_MIN is lower than REC_VERSION. */
#define REC_SNAP_START 0x70
#define REC_SNAP_END 0x71
/* An upper bound on a snapshot, so a length read out of a damaged file is refused
   rather than passed to malloc. A savegame runs to some tens of kilobytes; this is
   three orders of magnitude above that and still nowhere near a plausible allocation. */
#define REC_SNAP_MAX (16u * 1024u * 1024u)

static int s_teleWrite = 0;    /* this session records telemetry */
static int s_teleReported = 0; /* divergences named so far */
static int s_teleSaid = 0;     /* the one-off "telemetry on" line */
/* Asked for by this start rather than by the run. A mid-session start writes a snapshot
   and reloads, so the session begins a couple of ticks after the command that asked for
   it and the intent has to cross that gap; Record_Start sets it and record_begin is the
   only reader. */
static int s_teleAsked = 0;

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

static char s_headerBuf[REC_HEADER_MAX];

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
/* The version the file being replayed was written at, for the records whose shape it
   decides. Set before the range check, so a refused file never reaches a reader. */
static int s_recVersion = REC_VERSION;
static long s_cmdsRecorded = 0;
static long s_cmdsReplayed = 0;
static U32 s_prevMt = 0;
static long s_mtCalls = 0;
static U32 s_frameClock = 0;
static U32 s_baseline = 0;
static U32 s_baselineCarry = 0;
static char s_snapshot[ADELINE_MAX_PATH] = "-";
/* Whether the snapshot named above is going into the file rather than beside it. The
   header says which, so a reader that finds no chunk knows whether to look for a
   sibling or to accept that the recording carries no starting state at all. */
static int s_snapInline = 0;
/* Replay side: where the recording's own snapshot was staged, empty when it carries
   none. Held rather than rebuilt because the reload happens a tick after the file is
   read, and the path is what says the reload has a staged file to clean up. */
static char s_inlineSnap[ADELINE_MAX_PATH] = "";
/* The file this session is on, after a bare name has been resolved to the recordings
   folder. Reported rather than the string the player typed: with a default home, the
   two are usually not the same and only one of them says where to look. */
static char s_recPath[ADELINE_MAX_PATH] = "";
/* The last recording this run wrote, which is not s_recPath: that one moves to whatever
   was replayed last. Empty in a session that has only replayed. */
static char s_lastRecorded[ADELINE_MAX_PATH] = "";
/* The player's own session, saved where a playback takes it over, empty when there is
   nothing to go back to (a replay from the flag, which owns the whole run, or one started
   with no scene live). */
static char s_returnPoint[ADELINE_MAX_PATH] = "";
/* Set once the run is on its way out, so the teardown does not ask for work that needs a
   tick to happen in. */
static int s_atExit = 0;
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
/* The step the recorded session ran on, for a replay to pin the same one. 0 until a
   replay reads it out of the header. Carried rather than assumed, because a session
   recorded under `--fixed-dt 20` reproduces on 20 and on nothing else, and the default
   below would silently replay it at a different rate. */
static S32 s_reloadDt = 0;
/* Whether the step in force is one the recorder armed. Only then is it the recorder's to
   put back: a run given --fixed-dt is pinned for its whole length by something that is
   not this. */
static int s_recArmedDt = 0;
static int s_pendingBaseline = 0;
static int s_frameClockValid = 0;
static int s_hash32 = 0;    /* LBA2_REC_HASH32: store a 32-bit digest */
static int s_hashEvery = 1; /* LBA2_REC_HASH_EVERY: hash one tick in N */

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
static int s_kfReported = 0; /* keyframes reported since the mismatch, capped */

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

/* Digest of the binding table, so a recording knows which bindings resolved it.
   Its own FNV rather than the harness's state digest: this hashes the tables the file
   carries, not the game, and the two must be able to change without disturbing each
   other. */
static void bindings_mix(unsigned long long *h, S32 v) {
    const unsigned char *b = (const unsigned char *)&v;
    unsigned i;
    for (i = 0; i < sizeof v; i++) {
        *h ^= b[i];
        *h *= 1099511628211ULL;
    }
}

static unsigned long long bindings_digest(void) {
    unsigned long long h = 14695981039346656037ULL;
    S32 i;
    for (i = 0; i < MAX_INPUT_SLOTS; i++) {
        bindings_mix(&h, (S32)DefKeys[i].Key1);
        bindings_mix(&h, (S32)DefKeys[i].Key2);
        bindings_mix(&h, (S32)GamepadKeys[i].Key1);
        bindings_mix(&h, (S32)GamepadKeys[i].Key2);
    }
    return h;
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

/* The mode a recording was made under, and the settings that decide whether it can be
   reproduced. Written into the header and rebuilt live for comparison, so the two can
   never be assembled from different lists.

   The `numeric.` lines are the arithmetic a replay has to agree with, and they are
   named traits rather than a platform because the platform is not what decides it.
   Two of them so far:

   `numeric.rng` is the generator behind Rnd. Two builds drawing from different ones
   cannot reproduce each other, and libc's rand() is a different sequence with a
   different RAND_MAX per platform, which on its own is enough to make a recording
   unreplayable anywhere but the machine that made it.

   `numeric.long_double_bits` is LDBL_MANT_DIG, and it is here ahead of needing it. The
   projection, rotation and distance paths (LIB386/3D) compute in `long double` and
   round with lrintl, chosen to match x87's 80 bit registers: 64 mantissa bits on both
   x86-64 hosts, which is why Linux and Windows agree on them. ARM has no x87, so on
   macOS and Android `long double` is a plain double and the same expressions round
   differently. Recording the trait means such a replay opens by naming the arithmetic
   it disagrees about, instead of diverging somewhere in the middle and reading like a
   bug in the simulation. Making that case actually replay is a separate job, and it is
   a bigger one: it means taking those paths off the host's extended precision.

   The settings block is the Auto camera's, and it is there because it was measured:
   replaying a session against a config missing FollowCamera diverges at tick 0, and one
   missing FollowCamGroundClearance at tick 2 -- the camera rests at a different
   distance, and the digest covers the camera. Removing AllCameras, the mouse settings,
   Shadow or DetailLevel from the same config changed nothing. So the boundary is the
   Auto camera's own block rather than the whole of the config, and a setting joins it
   when a replay is shown to turn on it.

   `mode.audio` is there because whether audio came up decides what the simulation
   computes, in two places. SOURCES/AMBIANCE.CPP draws its random pan only
   `if (!IsSamplePlaying(...))`, from the one stream actor behaviour draws from, so one
   draw either way and the next actor to reach TM_ANGLE_RND turns to a different angle.
   And a dialogue with the text off spins on `TestSpeak()` until the voice sample ends
   (SOURCES/MESSAGE.CPP), which is worth hundreds of ticks rather than one draw. With no
   sample driver both answers are an unconditional no, which is a different branch and
   not a quieter one.

   `mode.verbose` is the same kind of answer for telemetry: whether this recording
   carries it, rather than whether the command line asked for it, since `rec start
   verbose` arms one session and the flag arms the run. Read outside a session -- `rec
   info` with no path -- there is no recording carrying anything, and it reports what one
   started now would carry.

   It records what the run got rather than what was asked for, because three routes
   reach the same place: `--no-audio`, `--headless` (which implies it), and a host where
   InitSampleDriver fails and the run continues silently. A recording made with sound
   cannot be reproduced by a replay without it, and since `--fixed-dt` requires
   `--headless` today, that is every replay. Naming the line is what turns that into a
   sentence at the top of the run instead of a divergence to be diagnosed.

   One line per key rather than a digest: `rec info` diffs the header line by line, so a
   named line reports the key that differs, and a digest could only report that
   something did.

   Returns 0 if the buffer was too small, because a truncated header silently drops the
   lines at the end of it, and the reader would report the setting as absent rather than
   as unread. */
static S32 write_mode_lines(char *out, size_t n) {
    int wrote = snprintf(out, n,
                         "engine=%s\n"
                         "mode.headless=%d\n"
                         "mode.audio=%d\n"
                         "mode.fixed_dt=%d\n"
                         "mode.verbose=%d\n"
                         "build.flags=%s\n"
                         "build.platform=%s\n"
                         "numeric.rng=%s\n"
                         "numeric.digest=%d\n"
                         "numeric.long_double_bits=%d\n"
                         "mode.resolution=%ux%u\n"
                         "mode.language=%d\n"
                         "setup.snapshot=%s\n"
                         "setup.reloaded=%d\n"
                         "setup.reload_clock=%u\n"
                         "clock.timer_ref_hr=%u\n"
                         "clock.sim_carry=%u\n"
                         "data.master=%s\n"
                         "bindings.digest=%016llx\n",
                         LBA2_VERSION_STRING,
                         Control_IsHeadless(),
                         Control_AudioActive(),
                         (int)Timer_FixedDtValue(),
                         s_teleWrite || Control_RecordTelemetry(),
                         BUILD_FLAGS_STR,
                         Sys_PlatformName(),
                         Rnd_StreamName(),
                         CONTROL_DIGEST_VERSION,
                         (int)LDBL_MANT_DIG,
                         (unsigned)ModeDesiredX, (unsigned)ModeDesiredY,
                         (int)Language,
                         s_snapInline ? "(inline)" : path_leaf(s_snapshot),
                         s_reloaded, s_reloadClock,
                         (unsigned)TimerRefHR,
                         (unsigned)LastSimRefHR,
                         Distrib_MasterName(Distrib_GetMaster()),
                         bindings_digest());
    if (wrote <= 0 || (size_t)wrote >= n)
        return 0;
    /* The rest comes from the engine, which this module deliberately cannot see. */
    return Control_DescribeState(out + wrote, n - (size_t)wrote);
}

/* ---- record --------------------------------------------------------------- */

static char s_armRecord[512];
static char s_armReplay[512];
/* Which of the two the flag asked for, rather than which of the two strings is not
   empty. The strings cannot answer it: an empty name is a name the recorder now picks
   for itself, so dispatching on `s_armRecord[0]` made `--record ""` arm the recorder and
   then match neither branch -- the run recorded nothing, said nothing, and exited 0.
   `--replay ""` was the worse half of the same bug, because a clean replay also exits 0,
   so a harness could not tell the two apart. 0 = nothing armed, 1 = record, 2 = replay. */
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

/* Where a recording lives when the player did not say.
 *
 * A name with no directory in it is a name in the recordings folder, which sits beside
 * the saves. That is the whole rule, and it runs the same way for reading and for
 * writing: a recording made as `session.rec` is replayed as `session.rec`, from
 * wherever the run happens to be started. Anything with a separator in it is a path and
 * is left alone, so `./session.rec` still means the one here.
 *
 * The exception is for reading only, and it is there because the alternative is a
 * "cannot open" for a file the player is looking straight at: a bare name the folder
 * does not have, but the working directory does, is taken from the working directory.
 * It cannot pick the wrong one, because it only applies where the folder has nothing.
 *
 * `n` is the size of `buf`, and is passed on rather than assumed: every caller here
 * hands over more than GetRecordingsPath will write, and the first one that does not
 * should get a short path rather than a smashed stack. */
static const char *rec_resolve(const char *path, char *buf, size_t n, int forWrite) {
    char home[512];
    char here[ADELINE_MAX_PATH];

    path = expand_home(path, home, sizeof home);
    if (path == NULL OR path[0] == 0) {
        /* Through `buf` rather than straight back out. `path` may be pointing at `home`
           by now, and handing a caller the address of a local that is about to go is a
           different kind of wrong from the empty name it was asked to resolve. */
        buf[0] = 0;
        return buf;
    }
    if (strchr(path, '/') != NULL OR strchr(path, '\\') != NULL) {
        snprintf(buf, n, "%s", path);
        return buf;
    }

    snprintf(here, sizeof here, "%s", path);
    GetRecordingsPath(buf, (U16)((n < ADELINE_MAX_PATH) ? n : ADELINE_MAX_PATH), here);
    if (!forWrite && !ExistsFileOrDir(buf) && ExistsFileOrDir(here))
        snprintf(buf, n, "%s", here);
    return buf;
}

/* A name for a recording nobody named.
 *
 * Pressing record must not require thinking of a filename first. That is true of the
 * console with no argument and it is the whole shape of the in-game button this is
 * groundwork for, which has nowhere to type one at all.
 *
 * Local time rather than a counter, so what accumulates in the folder reads as sessions
 * and sorts as they were played. Repeats inside one second are numbered rather than
 * left to collide: a driver on the control socket can ask twice faster than the stamp
 * moves, and the second recording would otherwise replace the first while both reported
 * success -- the same failure `unique_shoot_path` exists to prevent for screenshots.
 * Falling out of the loop leaves the last candidate, which is a hundred recordings in
 * one second and not a case worth a failure path. */
static void rec_auto_name(char *out, size_t n) {
    char stamp[32];
    char name[64];
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    int spare;

    if (lt == NULL || strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", lt) == 0)
        snprintf(stamp, sizeof stamp, "%lu", (unsigned long)now);

    for (spare = 0; spare < 100; spare++) {
        if (spare == 0)
            snprintf(name, sizeof name, "session-%s.rec", stamp);
        else
            snprintf(name, sizeof name, "session-%s-%d.rec", stamp, spare + 1);
        GetRecordingsPath(out, (U16)((n < ADELINE_MAX_PATH) ? n : ADELINE_MAX_PATH), name);
        if (!ExistsFileOrDir(out))
            return;
    }
}

/* The most recently written recording in the folder, for `rec play` with no argument.
 *
 * Modification time rather than the name. The auto-named ones happen to sort by time,
 * but a recording someone named sorts wherever its name puts it, and "play the one I
 * just made" has to mean the one just made or the no-argument form is a trap.
 *
 * Only names ending `.rec`, which is what the auto-name writes and what the docs use.
 * The staging savegame beside a live recording is `<leaf>.staging.lba` and does not
 * match, which is the point: it is in this folder precisely so the load menu cannot
 * see it, and it must not become something a player can replay either. */
static S32 rec_newest(char *out, size_t n) {
    char dir[ADELINE_MAX_PATH];
    char path[ADELINE_MAX_PATH];
    struct dirent *ent;
    DIR *d;
    time_t best = 0;
    S32 found = 0;

    GetRecordingsPath(dir, ADELINE_MAX_PATH, NULL);
    d = opendir(dir);
    if (d == NULL)
        return 0;
    while ((ent = readdir(d)) != NULL) {
        struct stat st;
        size_t len = strlen(ent->d_name);

        /* Searched in the last four characters alone, so this is a suffix test rather
           than a contains test: `notes.recovered.lba` holds `.rec` and is not one. */
        if (len < 4 || ade_strcasestr(ent->d_name + len - 4, ".rec") == NULL)
            continue;
        GetRecordingsPath(path, ADELINE_MAX_PATH, ent->d_name);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        /* A modification time is accurate to the second, so two recordings can share
           one. Break that by name, which does not identify the later of the two and
           does give the same answer every time this is asked: a folder that picked a
           different recording on each run would be the worse failure. Within one
           session the tie does not arise, because a recording this run made is
           preferred by name in Record_Play. */
        if (found && (st.st_mtime < best ||
                      (st.st_mtime == best && strcmp(path, out) <= 0)))
            continue;
        best = st.st_mtime;
        found = 1;
        snprintf(out, n, "%s", path);
    }
    closedir(d);
    return found;
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
    s_armed = 2;
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
static void record_save_to(const char *out) { Control_WriteSnapshot(out); }

/* Where a snapshot rests on its way into or out of the recording.
 *
 * The engine's save layer works in paths at both ends -- SaveGame writes GamePathname,
 * LoadGameNumCube reads it -- so a snapshot that lives inside the file still has to
 * touch the filesystem once on the way past. Teaching those two a buffer would be the
 * tidier answer and a far larger change, in the one part of the tree where the wire
 * format is pinned bit for bit.
 *
 * The recordings folder rather than the save folder, which is the obvious place and the
 * wrong one: the load menu lists every .LBA it finds there (SOURCES/GAMEMENU.CPP), so a
 * staging file would appear to the player as a save nobody made.
 *
 * Named for the recording it belongs to, which is the property the sibling savegames
 * had and the one thing keeping two engines on one user directory out of each other's
 * way. A single shared name lets a second engine overwrite the savegame the first is
 * about to reload, and a savegame from somebody else's session is a far quieter failure
 * than a missing file. Two runs recording to the same name are already contending for
 * the recording itself, so there is nothing left for this to protect. */
/* Staging under a name of the recording's, with `suffix` separating the two kinds: the
   snapshot a session is loaded from, and the player's own game held while a playback runs
   over it. Two names because a playback needs both at once and one file cannot be both. */
static void record_scratch_path_as(char *out, size_t n, const char *suffix) {
    char p[ADELINE_MAX_PATH];
    char name[ADELINE_MAX_PATH];
    snprintf(name, sizeof name, "%s.%s.lba",
             s_recPath[0] ? path_leaf(s_recPath) : "rec", suffix);
    GetRecordingsPath(p, ADELINE_MAX_PATH, name);
    snprintf(out, n, "%s", p);
}

static void record_scratch_path(char *out, size_t n) {
    record_scratch_path_as(out, n, "staging");
}

/* Stage a snapshot, and report whether one is actually there.
 *
 * The check is the point. Control_WriteSnapshot returns 1 whatever SaveGame did with
 * it, so a save that failed is indistinguishable from one that worked -- and the caller
 * goes on to reload the path it was given. Removing any earlier file first is what turns
 * that into a failure rather than a surprise: without it a run whose save failed reloads
 * whatever a previous run left behind, which is a live session replaced by an unrelated
 * one. */
static S32 record_write_snapshot_as(char *out, size_t n, const char *suffix) {
    record_scratch_path_as(out, n, suffix);
    remove(out);
    record_save_to(out);
    if (ExistsFileOrDir(out))
        return 1;
    fprintf(stderr, "[rec] could not write the snapshot at %s\n", out);
    out[0] = 0;
    return 0;
}

static S32 record_write_snapshot(char *out, size_t n) {
    return record_write_snapshot_as(out, n, "staging");
}

/* Copy a staged snapshot into the stream as a chunk, and drop the staging file.
 *
 * Deliberately outside the s_bytes accounting, which measures the record stream: a
 * report that folded a savegame into "bytes input" would say the input cost twenty
 * times what it does.
 *
 * Written whole and flushed, so the window in which this can tear is one write. What
 * survives a tear is the point of the frame rather than a hope about it: the length
 * goes down before the payload and is never revisited, so a file that stops in here
 * stops short of its tail and the reader below refuses it by name. */
static S32 record_write_chunk(U8 op, const char *file) {
    U8 head[REC_CHUNK_HEAD], tail[REC_CHUNK_TAIL];
    FILE *in;
    char *buf;
    long len = 0; /* named in the failure line below, which a short-circuiting fseek reaches */
    size_t got;
    S32 ok;

    if (!s_f)
        return 0;
    in = fopen(file, "rb");
    if (!in) {
        fprintf(stderr, "[rec] cannot read the snapshot at %s; it is not in the recording\n", file);
        return 0;
    }
    if (fseek(in, 0, SEEK_END) != 0 || (len = ftell(in)) <= 0 || (U32)len > REC_SNAP_MAX) {
        fclose(in);
        fprintf(stderr, "[rec] snapshot at %s is %ld bytes; not carrying it\n", file, len);
        return 0;
    }
    rewind(in);
    buf = (char *)malloc((size_t)len);
    if (!buf) {
        fclose(in);
        return 0;
    }
    got = fread(buf, 1, (size_t)len, in);
    fclose(in);
    if (got != (size_t)len) {
        free(buf);
        fprintf(stderr, "[rec] snapshot at %s read short; not carrying it\n", file);
        return 0;
    }

    RecFmt_PutChunkHead(head, sizeof head, op, (U32)len);
    RecFmt_PutChunkTail(tail, sizeof tail, (U32)len);
    ok = fwrite(head, 1, sizeof head, s_f) == sizeof head &&
         fwrite(buf, 1, (size_t)len, s_f) == (size_t)len &&
         fwrite(tail, 1, sizeof tail, s_f) == sizeof tail && fflush(s_f) == 0;
    free(buf);
    if (!ok) {
        /* The staged copy stays. A chunk the disk refused leaves a torn frame the
           reader will reject, and deleting the only other copy of that state would
           turn a recording missing its snapshot into one that never had it. */
        fprintf(stderr, "[rec] could not write the snapshot into the recording; "
                        "the staged copy is at %s\n",
                file);
        return 0;
    }
    remove(file);
    return 1;
}

/* The other end: lift a chunk out of the stream and stage it where the save loader can
   be pointed at it. The caller has already positioned the file at the chunk's head.

   `outPath` NULL reads the chunk without staging it, which is what a replay that is not
   going to reload wants: the stream still has to be stepped over, and writing a file
   nothing will open would leave it behind for somebody to find later and wonder about.

   A chunk whose tail does not close it is refused rather than staged. This is the case
   the frame exists for: a recording cut off mid-snapshot would otherwise hand the save
   loader a file that is a valid savegame right up to where it stops, and a replay would
   begin from a state the session never had. */
static S32 replay_read_chunk(U8 want, char *outPath, size_t n) {
    U8 head[REC_CHUNK_HEAD], tail[REC_CHUNK_TAIL];
    U8 op = 0;
    U32 len = 0;
    char *buf;
    FILE *out;
    S32 ok;

    if (fread(head, 1, sizeof head, s_f) != sizeof head)
        return 0;
    if (!RecFmt_GetChunkHead(head, sizeof head, &op, &len) || op != want)
        return 0;
    if (len == 0 || len > REC_SNAP_MAX) {
        fprintf(stderr, "[rec] snapshot in this recording claims %u bytes; refusing it\n",
                (unsigned)len);
        return 0;
    }
    buf = (char *)malloc((size_t)len);
    if (!buf)
        return 0;
    if (fread(buf, 1, (size_t)len, s_f) != (size_t)len ||
        fread(tail, 1, sizeof tail, s_f) != sizeof tail ||
        !RecFmt_ChunkTailOk(tail, sizeof tail, len)) {
        free(buf);
        fprintf(stderr, "[rec] the snapshot in this recording is incomplete; "
                        "the session it was made from did not finish writing it\n");
        return 0;
    }

    if (outPath == NULL) {
        free(buf);
        return 1;
    }

    record_scratch_path(outPath, n);
    out = fopen(outPath, "wb");
    if (!out) {
        free(buf);
        fprintf(stderr, "[rec] cannot stage the snapshot at %s\n", outPath);
        outPath[0] = 0;
        return 0;
    }
    ok = (fwrite(buf, 1, (size_t)len, out) == (size_t)len);
    if (fclose(out) != 0)
        ok = 0;
    free(buf);
    if (!ok) {
        fprintf(stderr, "[rec] could not write the staged snapshot at %s\n", outPath);
        remove(outPath);
        outPath[0] = 0;
    }
    return ok;
}

/* Registered by both a recording and a replay, and it does two things rather than one.
 *
 * Record_Stop flushes the stream, which is why a recording wants it: a crash runs no
 * atexit handler but a menu quit does, and one tick of loss beats a whole session.
 *
 * The binding restore is why a *replay* needs it, and that one is not housekeeping. A
 * replay installs the recording's key tables so the file resolves against the bindings
 * it was made under. If the run ends before the stream does -- `--tick N --exit`, the
 * window closed, anything -- Record_Stop never runs, and the normal exit path writes
 * lba2.cfg from whatever DefKeys holds. That is the recording's bindings, written
 * permanently over the player's own. Restored here unconditionally, because Record_Stop
 * returns early once the stream is closed and would skip it. */
static void record_flush_at_exit(void) {
    /* No ticks left, so nothing can issue a reload and there is no game to return to.
       Without this the exit path asks for one anyway and says so, which is a line
       promising something that will not happen. The return point is removed below
       instead. */
    s_atExit = 1;
    Record_Stop();
    bindings_restore();
    /* A replay that ended before its reload landed still has the snapshot staged. The
       reload path drops it as soon as the load has read it; this is the other way out,
       and it is here for the same reason the binding restore is: the run can stop at
       any point, and what it leaves on disk should not depend on where. */
    if (s_inlineSnap[0]) {
        remove(s_inlineSnap);
        s_inlineSnap[0] = 0;
    }
    /* The player's session, if the run ends before it could be put back. Removing it is
       all that is left to do here: there are no more ticks, so the reload Record_Stop
       asked for is never going to be issued. */
    if (s_returnPoint[0]) {
        remove(s_returnPoint);
        s_returnPoint[0] = 0;
    }
}

/* atexit once per run, whichever end armed it first. */
/* Refuse to write or compare a keyframe the local buffers cannot hold. Says so once:
   the alternative is either a stack overrun or a silent stop, and a keyframe is
   diagnosis, so losing it costs nothing else. */
static int record_keyframe_fit(void) {
    static int said = 0;
    if (Control_KeyframeCount() <= REC_KF_MAX)
        return 1;
    if (!said) {
        said = 1;
        fprintf(stderr, "[rec] keyframe has %d fields, this build holds %d; not recording it\n",
                (int)Control_KeyframeCount(), (int)REC_KF_MAX);
    }
    return 0;
}

static void record_arm_atexit(void) {
    static int armed = 0;
    if (!armed) {
        armed = 1;
        atexit(record_flush_at_exit);
    }
}

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
/* 0 = begin recording after it lands, 1 = begin replay, 2 = restore and begin nothing.
   The third is the way back out of a playback: a replay loads the recording's world over
   the player's, and the player's has to be put back afterwards or watching a recording
   costs them their session. */
#define REC_RELOAD_RECORD 0
#define REC_RELOAD_PLAY 1
#define REC_RELOAD_RESTORE 2
static int s_reloadForPlay = REC_RELOAD_RECORD;
static int s_reloadState = 0; /* 1 = asked for, 2 = issued and waiting for the cube */
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
    /* Cleared here so the step belongs to the request rather than to the run: a session
       that replays a recording and then starts one of its own would otherwise still be
       holding the replayed recording's step. Nothing reads it on this path today --
       Timer_FixedDtActive() is true by then and the arming is skipped -- so this is
       keeping the invariant local rather than resting it on a guard somewhere else.
       Record_Play sets it from the header immediately after calling this. */
    s_reloadDt = 0;
    s_reloadState = 1;
}

/* The step a recording pins when the run did not ask for one. 16 ms is what the harness
   and every fixture use, and what docs/RECORDING.md tells a player to pass. */
#define REC_DEFAULT_DT 16

static void record_reload_issue(void) {
    /* Pin the step here, which is the one place a mid-session recording can.
     *
     * Timer_EnableFixedDt zeroes TimerRefHR, and that is only safe where a load follows
     * to put the clock back -- which is exactly what this function is about to do. It is
     * the same order Control_Begin uses on the --load path (arm, then load), and it is
     * why arming here works where the two points that were tried before did not: the
     * first input poll and the end of argument parsing have no load behind them, so the
     * zeroed clock is what the session then runs on.
     *
     * Both ends, so a recording and its replay reach the load the same way. Only when the
     * run did not already ask for a step: re-arming an active one would reset the clock
     * of a session that is already pinned and reproducing.
     *
     * The replay takes the step out of the recording's header rather than the default,
     * so a session played at some other step replays at that one. */
    if (s_reloadForPlay != REC_RELOAD_RESTORE && !Timer_FixedDtActive()) {
        Timer_EnableFixedDt((U32)(s_reloadDt > 0 ? s_reloadDt : REC_DEFAULT_DT));
        s_recArmedDt = 1;
    }

    /* Before the load, because the load is what reads it. A savegame restores the
       fields the hash covers and re-derives the rest, and some of what it re-derives is
       stamped from the game clock; the two ends do not arrive here with the same
       reading, since writing the snapshot costs the recording a frame the replay never
       spends. Same state at tick 0 either way, and one actor's heading apart 300 ticks
       later, which is what this closes. */
    /* A restore is going back to ordinary play, where the clock is the host's again and
       nothing is being reproduced, so it neither carries a reading nor stamps one. */
    if (s_reloadForPlay == REC_RELOAD_PLAY)
        SetTimerHR((U32)s_reloadClock);
    else if (s_reloadForPlay == REC_RELOAD_RECORD)
        s_reloadClock = (unsigned)TimerRefHR;

    Control_BorrowSaveContext();
    Control_RequestLoad(s_reloadSave);
}

static void record_reload_landed(void) {
    Control_ClearLoadFlag();

    /* Deliberately no reseed here. ChangeCube is the engine's only Rnd_Seed and outside
       the demo reel it seeds from TimerRefHR (SOURCES/OBJECT.CPP), so two ends that
       reach the cube change at different readings draw different streams. Pinning the
       clock before the load settles that at its cause: both ends reach the cube change
       at the same TimerRefHR, so the seed follows without being carried. That also holds
       for cube changes later in a session, which a seed named in the header could not
       reach. */

    /* The recorder does not get to change which save the session is on. */
    Control_ReturnSaveContext();

    /* The staged copy has served its purpose: LoadGameNumCube read the whole file when
       the load was requested, and the state came out of the buffer it left behind. Only
       ever the staging path, never a save somebody named. */
    if (s_inlineSnap[0] && !strcmp(s_reloadSave, s_inlineSnap)) {
        remove(s_inlineSnap);
        s_inlineSnap[0] = 0;
    }
    /* Same for the player's own session once it has been put back. */
    if (s_returnPoint[0] && !strcmp(s_reloadSave, s_returnPoint)) {
        remove(s_returnPoint);
        s_returnPoint[0] = 0;
    }
}

/* `haveSnapshot` for the reload path, which wrote and then loaded the snapshot before
   getting here: re-saving it now would name a file the replay never loads, and the
   state a replay has to reach is the one this run reached from the file it took. */
static int record_begin(const char *path, int haveSnapshot) {
    char buf[512];
    if (s_recording || s_replaying)
        return 0;
    path = rec_resolve(path, buf, sizeof buf, 1);
    s_f = fopen(path, "wb");
    if (!s_f) {
        Console_Print("rec: cannot write %s", path);
        /* With the ask that came in, for the same reason the refusal in Record_Start
           clears it: a start that did not happen must not leave `verbose` behind for the
           next one, which would then record telemetry nobody asked it for. */
        s_teleAsked = 0;
        return 0;
    }
    /* Behind the open, so Record_Path never names a session that does not exist. On the
       mid-session path Record_Start has already set this to the same string, which is
       where the staging savegame took its name from. */
    snprintf(s_recPath, sizeof s_recPath, "%s", path);
    /* What `rec play` with no argument means, when this session made one: the recording
       this run recorded, exactly, rather than the newest file that happens to be in the
       folder. Both answer the same in the ordinary case; this one cannot be wrong about
       it, and the scan can, because a modification time is only accurate to the second
       and two short recordings can share one. */
    snprintf(s_lastRecorded, sizeof s_lastRecorded, "%s", path);

    char mode[REC_HEADER_MAX];
    if (!haveSnapshot)
        record_write_snapshot(s_snapshot, sizeof s_snapshot);
    s_snapInline = 1;
    /* Not fatal here, unlike the mid-session path above: a from-boot recording reloads
       nothing, so a snapshot it could not write costs it the state it started from and
       not the session. record_write_chunk says so and the header still records that the
       recording claims to carry one, which is what the reader reports finding absent. */
    /* A player quits through the menu, a crash does not quit at all: flush on any exit
       rather than only on `rec stop`. */
    record_arm_atexit();
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
    /* Telemetry is a diagnostic recording, several megabytes larger, made deliberately
       when a plain one has already said a tick and cannot say a field. Either way of
       asking arms it: --record-telemetry for the whole run, `rec start verbose` for this
       session. Before the header, because the header reports it. */
    s_teleWrite = s_teleAsked || Control_RecordTelemetry();
    s_teleAsked = 0;
    s_teleSaid = 0;
    if (!write_mode_lines(mode, sizeof mode))
        fprintf(stderr, "[rec] header did not fit; the mode lines at the end of it are missing\n");
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

    /* The snapshot goes in before the first record, so a reader meets it where the
       stream would otherwise begin and never has to hunt for it. It is also the only
       order a forward-only writer can produce: a session's opening state is the one
       thing about it that is known before it starts. */
    record_write_chunk(REC_SNAP_START, s_snapshot);

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

    /* Hold the game to real time for as long as this recording lasts. Asked of the timer
       rather than done here per tick: the clock hands out a step from three places and
       only one of them is the tick, so pacing from this hook left every modal loop --
       fades, menus, dialogue -- taking virtual time for free. */
    Timer_SetFixedDtPaced(1);
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
    /* Answer the simulation's sample-playing question from the game clock for the whole
       session. Without it a voice's lifetime is the audio device's, which no replay
       reproduces, and AMBIANCE.CPP's `if (!IsSamplePlaying(...))` pan draw comes off the
       one random stream actor behaviour uses. Armed for a host-sampled session too, not
       just a pinned one: a loose recording needs it more, having nothing else holding the
       two ends together. */
    Timer_SetSimAudioClock(1);
    /* Back to this build's field set. A replay lowers it to whatever the file it opened
       declared, and a session that played an old recording and then started a new one
       would otherwise write numeric.digest=2 in the header over hashes computed the old
       way -- a file that lies about itself, and mismatches on tick 0 of its own replay
       forever. */
    Control_SetDigestVersion(CONTROL_DIGEST_VERSION);
    s_recording = 1;
    return 1;
}

int Record_Start(const char *path, int verbose) {
    char buf[512];
    char snap[ADELINE_MAX_PATH];
    char named[ADELINE_MAX_PATH];

    if (s_recording || s_replaying || s_reloadState)
        return 0;

    /* After the refusal above, so a start that was turned away does not leave the ask
       behind for the next one to pick up. */
    s_teleAsked = verbose;

    /* No name means one of ours. Here rather than in the console, because every way of
       asking for a recording arrives through this function and they should all land in
       the same place: the command with no argument, the flag with no value, and the
       button that has no argument to give. */
    if (path == NULL || path[0] == 0) {
        rec_auto_name(named, sizeof named);
        path = named;
    }

    /* From the flag, and from anywhere there is no live scene to snapshot, record where
       we stand. Everything else is a mid-session start, which has no setup unless it
       makes one: write the snapshot, reload it, and let the first recorded frame be the
       first post-load one, so a replay reading this file lands where this run did.
       The marker describes this start, not the run: a session that recorded from the
       flag and then stopped can still start a second, mid-session recording by hand. */
    if (s_recordFromCli || !Control_HasLiveScene()) {
        s_recordFromCli = 0;
        s_reloaded = 0;
        return record_begin(path, 0);
    }

    path = rec_resolve(path, buf, sizeof buf, 1);
    /* Before the snapshot, which is named after it, and before the reload: record_begin
       runs a tick later and the caller reports where it is recording as soon as this
       returns. */
    snprintf(s_recPath, sizeof s_recPath, "%s", path);
    if (!record_write_snapshot(snap, sizeof snap)) {
        /* No reload without a snapshot to reload. Handing the load path a name nothing
           wrote is how a failed save becomes a scene change nobody asked for. */
        Console_Print("rec: cannot write the starting snapshot; not recording");
        s_recPath[0] = 0;
        s_teleAsked = 0;
        return 0;
    }
    snprintf(s_snapshot, sizeof s_snapshot, "%s", snap);
    record_reload_request(snap, path, REC_RELOAD_RECORD);
    Console_Print("rec: reloading %s; recording starts from the post-load state", snap);
    return 1;
}

/* Give the simulation step back, if it was the recorder that took it.
 *
 * A pinned step advances game time by dt per rendered frame rather than by wall clock,
 * and the recorder paces frames to match only while it is recording. So a session that
 * kept the step after `rec stop` ran at whatever rate it rendered at: measured headless,
 * 1.00x real time before a recording and 3.12x after one. On a vsynced 60 Hz window it
 * would read as roughly right and on a 144 Hz one as more than twice too fast, which is
 * the worse failure of the two -- it looks like the game, only wrong.
 *
 * Not for a run given --fixed-dt: there the step belongs to the whole run and to whoever
 * asked for it, and this is only ever putting back what a mid-session `rec start` took. */
static void record_release_step(void) {
    /* The frame clock belongs to the session rather than to the step, and it has to go
       first. While fixed-dt is armed Record_ClockHook returns before ever reading it, so
       a stale one is invisible -- until the step is handed back, and then it pins
       ManageTime at whatever reading the session ended on and game time stops dead.
       Measured on the way to this: 3.12x real time before clearing it, 0.00x after
       handing back the step without. Cleared for every session, armed or not, so the
       two cannot come apart again. */
    s_frameClock = 0;
    s_frameClockValid = 0;
    Timer_SetSimAudioClock(0);
    /* Unconditionally, like the two above and for the same reason: a run given --fixed-dt
       keeps the step after `rec stop` but has no reason to keep being held to real time,
       and leaving it armed would slow a harness run to a crawl with nothing saying why. */
    Timer_SetFixedDtPaced(0);

    if (!s_recArmedDt)
        return;
    s_recArmedDt = 0;
    Timer_DisableFixedDt();
}

/* Put the player back where the playback found them.
 *
 * The same pair `rec start` uses, in the other direction: a snapshot, then a reload. It
 * runs from Record_Stop, so it covers both ways a playback ends -- the replay reaching
 * the end of its stream, and the player stopping it.
 *
 * Not while another reload is in flight. The state machine holds one request, and a
 * playback stopped before its own reload landed is already committed to that load; a
 * second request would replace the first and strand FlagLoadGame, which is the one thing
 * about this path that must not be got wrong. The file is left for the exit cleanup. */
static void record_restore_player(void) {
    if (!s_returnPoint[0] || s_reloadState || s_atExit)
        return;
    record_reload_request(s_returnPoint, "", REC_RELOAD_RESTORE);
    Console_Print("rec: returning to your game");
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
        if (!s_f) {
            record_release_step();
            return 1;
        }
    }

    if (!s_f) {
        record_release_step();
        return 0;
    }

    /* A second snapshot where the recording ends, on the same mechanics as the one at
       the start. The per-tick hash says which tick a replay stopped matching; this says
       what it ended up with, in fields rather than in a digest, and it is the thing a
       replay's final state can be diffed against directly.
     *
     * Only while a scene is live, and only for a recording: a run that ended in a crash
     * leaves no end snapshot, which is the correct outcome rather than a gap to paper
     * over. Its absence says the session did not finish, which is exactly what a crash
     * repro wants to record.
     *
     * Ahead of the close, because the snapshot goes into the file the close is closing.
     * Writing the save is ordinary engine work -- SaveGame opens its own handle -- so it
     * does not touch the stream that is still open here. The stream is flushed first all the same: this runs from
     * atexit as well as from `rec stop`, and it puts a scene serialisation between the
     * last poll written and the close that would otherwise have committed it. */
    if (wasRecording && Control_HasLiveScene()) {
        fflush(s_f);
        if (record_write_snapshot(endPath, sizeof endPath))
            record_write_chunk(REC_SNAP_END, endPath);
    }

    fclose(s_f);
    s_f = NULL;
    s_recording = 0;
    s_replaying = 0;
    /* With the session, not with the run: the next `rec start` decides for itself, and a
       header written from outside a session should not still be reporting this one's. */
    s_teleWrite = 0;
    s_snapInline = 0;
    s_recPath[0] = 0;
    bindings_restore();
    record_release_step();
    record_restore_player();
    return 1;
}

static void record_poll(void) {
    U8 flags = 0;
    U16 changed[TABKEYS_NUM_KEYS];
    U16 nchg = 0;
    U16 i;
    U32 src = Timer_ClockSource();
    U32 delta = src - s_prevSrc;
    S32 mute;
    S32 key;
    s_frameClock = src;
    s_frameClockValid = 1;

    /* Record what gameplay is about to be given, which is nothing while the console is
       open: SOURCES/INPUT.CPP clears keys, pad and mouse so typing cannot drive the hero.
       This hook samples upstream of that, so without the mute a recording carries the
       keystrokes of whatever was typed at the console, and a replay -- which has no
       console open to swallow them -- presses them into the game. Measured on a real
       session: typing `exit` and Return went into the file as scancodes 8, 12, 23, 27 and
       40, two of them bound to actions in that player's own config. */
    mute = Console_IsOpen();
    key = mute ? 0 : Key;

    for (i = 0; i < TABKEYS_NUM_KEYS; i++) {
        U8 v = mute ? (U8)0 : TabKeys[i];
        if (v != s_prevKeys[i]) {
            changed[nchg++] = i;
        }
    }
    if (nchg)
        flags |= 0x01;
    if (key != s_prevKey)
        flags |= 0x02;

    S16 rsx = 0, rsy = 0;
    JoystickGetRightStick(&rsx, &rsy);
    U32 padFirst = JoystickFirstPressedScancode();
    S32 mdx, mdy, click = Click;
    /* The motion ManageMouse has yet to deliver, not the motion it delivered last time.
       This hook runs after ManageEvents has pumped the frame's events and before the
       camera's ManageMouse drains them, so MouseXDep here is still the previous frame's:
       capturing it records every mouse movement one poll late, and a replay orbits a
       frame behind the session it is reproducing. */
    PeekMouseMotion(&mdx, &mdy);
    if (mute) {
        rsx = rsy = 0;
        padFirst = 0;
        mdx = mdy = click = 0;
    }
    if (rsx || rsy || padFirst || mdx || mdy || click)
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
            put8(mute ? (U8)0 : TabKeys[changed[i]]);
        }
    }
    if (flags & 0x02)
        put32((U32)key);
    if (flags & 0x04) {
        put16((U16)rsx);
        put16((U16)rsy);
        put32(padFirst);
        put32((U32)mdx);
        put32((U32)mdy);
        put32((U32)click);
    }
    if (flags & 0x08)
        put32(delta);

    if (flags == 0)
        s_quietPolls++;

    if (mute)
        memset(s_prevKeys, 0, sizeof s_prevKeys);
    else
        memcpy(s_prevKeys, TabKeys, sizeof s_prevKeys);
    s_prevKey = key;
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
/* How much the file holds, read off the last sync marker rather than by scanning the
   whole stream. Printed when a replay starts, because otherwise the only way to size
   `--tick` is to guess: guess low and the run ends before the stream does, which prints
   no summary and reads exactly like a pass, and guess high and it is minutes of waiting
   to find that out. */
/* Is `line` present in `hay` as a whole line? `strstr` alone says yes for
   `setup.cube=1` inside `setup.cube=12`, and reports a genuine mode difference as
   matching -- exactly the wrong way round for a check whose job is to warn. */
static int line_present(const char *hay, const char *line) {
    size_t len = strlen(line);
    const char *p = hay;

    if (len == 0)
        return 1;
    while ((p = strstr(p, line)) != NULL) {
        int startsLine = (p == hay) || (p[-1] == '\n');
        char after = p[len];
        if (startsLine && (after == 0 || after == '\n'))
            return 1;
        p += len;
    }
    return 0;
}

/* Lines whose difference is not a warning. Everything else in the header describes
   something a replay depends on, so the default is to report it. */
static int mode_line_ignored(const char *line) {
    static const char *const skip[] = {
        "clock.timer_ref_hr=", /* the recorded baseline: restored, not matched */
        "clock.sim_carry=",    /* the sub-step carry: restored with it */
        "setup.snapshot=",     /* the file it started from, named per recording */
        "setup.reloaded=",     /* how it started, which the reader acts on rather than matches */
        "setup.reload_clock=",
        "bindings.keyboard=", /* payload a replay installs; bindings.digest is the comparison */
        "bindings.gamepad=",
        "mode.verbose=", /* telemetry is diagnosis, and does not reach the simulation */
        /* Provenance, not contract. A cross-platform replay is the point, so this line
           differing is the expected case; the `numeric.` lines are what decide whether
           the two platforms compute the same game. */
        "build.platform=",
    };
    S32 i;

    for (i = 0; i < (S32)(sizeof skip / sizeof skip[0]); i++) {
        if (!strncmp(line, skip[i], strlen(skip[i])))
            return 1;
    }
    return 0;
}

/* What this run does not match about the recording, said at the point the replay
   starts rather than left to be discovered as a divergence a thousand ticks later.
   `rec info` answers the same question on demand; this is the same comparison for
   somebody who never thought to ask it, which is the common case: the settings a
   session was played under are not part of the save, and a config edited between the
   recording and the replay reads as the simulation diverging. */
static void replay_report_mode(void) {
    char live[REC_HEADER_MAX];
    char hdr[REC_HEADER_MAX];
    char *p, *nl;
    S32 diffs = 0;

    /* Said rather than returned quietly. This is the run's whole account of what it does
       not match, so skipping it looks exactly like a run with nothing to report. */
    if (!write_mode_lines(live, sizeof live)) {
        fprintf(stderr, "[rec] this run's header did not fit, so nothing was compared "
                        "against the recording's\n");
        return;
    }
    snprintf(hdr, sizeof hdr, "%s", s_headerBuf);

    for (p = hdr; (nl = strchr(p, '\n')) != NULL; p = nl + 1) {
        char key[64];
        char mine[128];
        const char *eq;
        size_t klen;

        *nl = 0;
        /* The magic line has no `=`, and neither would anything else that is not a
           field; there is nothing to compare it against. */
        eq = strchr(p, '=');
        if (eq == NULL || mode_line_ignored(p) || line_present(live, p))
            continue;

        /* Name what this run has instead, so the line is a comparison rather than an
           accusation the reader has to go and check. */
        klen = (size_t)(eq - p) + 1;
        if (klen >= sizeof key) {
            snprintf(mine, sizeof mine, "?");
        } else {
            memcpy(key, p, klen);
            key[klen] = 0;
            if (!RecFmt_ReadStr(live, key, mine, sizeof mine))
                snprintf(mine, sizeof mine, "absent");
        }
        fprintf(stderr, "[rec] mode differs: %s (this run: %s)\n", p, mine);
        diffs++;
    }

    if (diffs)
        fprintf(stderr, "[rec] %d mode line(s) differ; this replay may not reproduce\n",
                (int)diffs);
}

static void report_extent(void) {
    long here = ftell(s_f);
    long end, from, n, i;
    unsigned char *buf;
    long bestPoll = -1, bestTick = -1;

    if (fseek(s_f, 0, SEEK_END) != 0)
        return;
    end = ftell(s_f);

    /* Stop short of the end snapshot, if there is one. A savegame is arbitrary bytes
       and the marker this scans for is five of them, so leaving it in range would
       eventually let a snapshot spell out a sync marker and report an extent from
       whatever numbers followed it. The trailer is what makes that avoidable: its
       length sits behind the payload, so the chunk can be found from the end of the
       file without parsing forward through the stream. */
    {
        U8 tail[REC_CHUNK_TAIL];
        U32 len = 0;
        /* REC_SNAP_MAX before the cast, because `long` is 32 bits on Windows and a
           length above 2^31 out of a damaged file would turn into a negative offset and
           push `end` past EOF. Every other chunk read here bounds the length the same
           way; this one is reached from the end of the file rather than from a head, so
           it has no opcode in front of it to disagree with first. */
        if (end > (long)sizeof tail && fseek(s_f, end - (long)sizeof tail, SEEK_SET) == 0 &&
            fread(tail, 1, sizeof tail, s_f) == sizeof tail &&
            RecFmt_ReadChunkTailLen(tail, sizeof tail, &len) && len <= REC_SNAP_MAX) {
            long at = end - (long)sizeof tail - (long)len - REC_CHUNK_HEAD;
            U8 op = 0;
            /* And the head has to be where the tail says it is, so a stream that
               happens to end in the magic does not move the scan. */
            if (at > 0 && fseek(s_f, at, SEEK_SET) == 0 && fread(&op, 1, 1, s_f) == 1 &&
                op == REC_SNAP_END)
                end = at;
        }
    }

    from = (end > 65536) ? end - 65536 : 0;
    n = end - from;
    if (n < 13) {
        fseek(s_f, here, SEEK_SET);
        return;
    }

    buf = (unsigned char *)malloc((size_t)n);
    if (buf == NULL) {
        fseek(s_f, here, SEEK_SET);
        return;
    }
    fseek(s_f, from, SEEK_SET);
    n = (long)fread(buf, 1, (size_t)n, s_f);

    /* Last marker wins, so walk forward and keep overwriting. */
    for (i = 0; i + 12 < n; i++) {
        U32 magic;
        if (buf[i] != REC_SYNC)
            continue;
        magic = (U32)buf[i + 1] | ((U32)buf[i + 2] << 8) | ((U32)buf[i + 3] << 16) |
                ((U32)buf[i + 4] << 24);
        if (magic != REC_SYNC_MAGIC)
            continue;
        bestPoll = (long)((U32)buf[i + 5] | ((U32)buf[i + 6] << 8) | ((U32)buf[i + 7] << 16) |
                          ((U32)buf[i + 8] << 24));
        bestTick = (long)((U32)buf[i + 9] | ((U32)buf[i + 10] << 8) | ((U32)buf[i + 11] << 16) |
                          ((U32)buf[i + 12] << 24));
    }
    free(buf);
    fseek(s_f, here, SEEK_SET);

    if (bestTick >= 0)
        fprintf(stderr, "[rec] holds about %ld ticks over %ld polls; give --tick more than that\n",
                bestTick, bestPoll);
}

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

/* How many fields this file's keyframe carries. Versions 9 to 11 wrote no count and
   always 23; 12 writes one, so a field can be added to the keyframe from here on without
   changing what a reader has to know. Consumes the count when the file has one, so the
   caller is left positioned on the first field either way. */
static int replay_keyframe_count(S32 *out) {
    U16 n;
    if (s_recVersion < 12) {
        *out = REC_KF_LEGACY_FIELDS;
        return 1;
    }
    if (!get16(&n))
        return 0;
    /* Whatever it says, including more than this build has names or room for. A count is
       how many values follow, not how many are wanted, and a reader that takes fewer
       leaves the rest to be read as the next record. */
    *out = (S32)n;
    return 1;
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

/* Put the state the file last described back over whatever UpdateKeyboardState just
   read from the host. Every consumer downstream of the funnel reads these and cannot
   tell they were replayed.
 *
 * Called on the paths that return without decoding a poll record too. Those frames
 * still run: skipping the injection leaves the live keyboard in place, so a replay
 * silently takes the operator's input for a frame, which is the one thing a replay must
 * never do. They do not advance s_polls, because no poll was consumed from the file. */
static void replay_inject(void) {
    memcpy(TabKeys, s_prevKeys, sizeof s_prevKeys);
    Key = s_prevKey;
    Joystick_SetReplayState(s_padFirst, s_rsx, s_rsy);
    /* Restoring the axes is only half of it: the analog camera asks whether a pad is
       there before it reads them, so a pad session replayed on a machine with none puts
       the values back into a run that never looks. Presence is taken from the axes
       rather than carried in the header, because the header is written before the
       session starts and the only thing gated on presence does nothing with a centred
       stick, so a poll that carries no deflection and a poll with no pad behind it are
       the same poll as far as the game is concerned. */
    Joystick_SetVirtualPad(s_rsx != 0 || s_rsy != 0);
    /* Where a motion event would have put it, so ManageMouse drains it on this frame's
       own schedule. Overwrites rather than adds: a mouse moved over the window during a
       replay must not reach the game. */
    SetMouseMotion(s_mdx, s_mdy);
    Click = s_click;
}

static int replay_poll(void) {
    U8 flags;
    long atFlags = ftell(s_f);
    long stepped = -1;

    if (!get8(&flags)) {
        s_eof = 1;
        return 0;
    }
    /* The end snapshot is the end of the stream. Reached here rather than at EOF for a
       session that stopped cleanly, and it means the same thing: the recording has run
       out of input. The chunk itself is not read -- nothing in a replay consumes the
       state the session finished at, and a reader that wants it works backwards from
       the end of the file. */
    if (flags == REC_SNAP_END) {
        s_eof = 1;
        return 0;
    }
    if (flags > 0x0F && flags != REC_SYNC && flags != REC_KEY && flags != REC_TELE &&
        flags != 0x40 && flags != REC_SNAP_START && !(flags & 0x80)) {
        if (!replay_resync()) {
            s_eof = 1;
            return 0;
        }
        replay_inject();
        return 1;
    }
    if (!replay_take_sync(&flags)) {
        s_eof = 1;
        return 0;
    }

    /* Records that are not a poll can sit anywhere between two polls, so step over each
       and take the next record as this poll rather than reading one as input.
       A keyframe and its telemetry follow a tick record and the tick path reads them
       there; a command is written where it was issued, which for the harness is just
       after the tick record and for anything typed at the console is in the middle of a
       poll run, because the console executes from the input path. Treating that as the
       stream being out of step ended a replay on the first command the player typed. */
    for (;;) {
        if (flags == REC_KEY) {
            U32 kt, kv;
            S32 n, k;
            if (!get32(&kt) || !replay_keyframe_count(&n))
                return 0;
            for (k = 0; k < n; k++) {
                if (!get32(&kv))
                    return 0;
            }
        } else if (flags == REC_TELE) {
            U32 tt, tv;
            U16 tn, k;
            if (!get32(&tt) || !get16(&tn))
                return 0;
            for (k = 0; k < tn; k++) {
                if (!get32(&tv))
                    return 0;
            }
        } else if (flags == REC_SNAP_START) {
            /* Not where one is written, and stepped over rather than resynced past all
               the same: the length is right there, so skipping it costs nothing and
               keeps a reader that meets an unexpected chunk on the stream instead of
               hunting for the next sync marker 64 polls later. */
            U32 len = 0;
            if (!get32(&len) || len > REC_SNAP_MAX ||
                fseek(s_f, (long)len + REC_CHUNK_TAIL, SEEK_CUR) != 0) {
                s_eof = 1;
                return 0;
            }
        } else if (flags == 0x40) {
            /* Queued rather than run here: executing a command inside the keyboard path
               re-enters this reader. It runs at the next tick boundary instead, so a
               command typed during a tick lands one tick later than it did live. */
            U32 ct;
            U16 n, j;
            char line[sizeof s_cmds[0].line];
            if (!get32(&ct) || !get16(&n))
                return 0;
            for (j = 0; j < n; j++) {
                U8 c;
                if (!get8(&c))
                    return 0;
                if ((size_t)(j + 1) < sizeof line)
                    line[j] = (char)c;
            }
            line[((size_t)(n + 1) < sizeof line) ? n : sizeof line - 1] = 0;
            if (s_nCmds < REC_CMD_MAX) {
                s_cmds[s_nCmds].tick = ct;
                snprintf(s_cmds[s_nCmds].line, sizeof s_cmds[s_nCmds].line, "%s", line);
                s_nCmds++;
            }
        } else {
            break;
        }
        stepped = ftell(s_f);
        if (!get8(&flags)) {
            s_eof = 1;
            return 0;
        }
        /* The same end-of-stream test as the one above, because this is a second place
           the flags byte is read and the end chunk can land behind any record this loop
           steps over. Without it 0x71 falls out of the loop into the poll decoder, and
           `flags & 0x01` is true of 0x71: the chunk's length bytes would be read as a
           count of changed keys and the savegame behind them as the keys themselves,
           which is a replay pressing input the recording never held. */
        if (flags == REC_SNAP_END) {
            s_eof = 1;
            return 0;
        }
        if (!replay_take_sync(&flags)) {
            s_eof = 1;
            return 0;
        }
        /* Uncovered this tick's own record. It belongs to the tick path, so put it back
           and let this poll pass with the input unchanged. */
        if ((flags & 0x80) && stepped >= 0) {
            fseek(s_f, stepped, SEEK_SET);
            replay_inject();
            return 1;
        }
    }
    if (flags & 0x80) {
        /* A tick record where a poll was expected: the engine has polled fewer times
           than the recording did. Put it back rather than read it here -- it is the
           tick path's record, it is 16 bytes rather than the 12 this once consumed,
           and reading it 4 bytes short left the reader decoding hash bytes as input
           until the next sync marker. */
        fseek(s_f, atFlags, SEEK_SET);
        replay_inject();
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
        LastSimRefHR = s_baselineCarry;
    }

    replay_inject();

    s_polls++;
    s_pollsThisTick++;
    return 1;
}

/* The counters, sampled where the replay actually starts rather than where it was
   asked for: a replay that reloads first takes a scene load between the two, and the
   clock it compares against has to be the one it begins on. */
static void replay_begin(void) {
    Timer_SetSimAudioClock(1);
    memset(s_prevKeys, 0, sizeof s_prevKeys);
    s_prevKey = 0;
    s_prevSrcDelta = 0;
    s_replaySrc = Timer_ClockSource();
    s_refDriftMax = 0;
    s_refDriftFirstTick = -1;
    s_teleReported = 0;
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
    record_arm_atexit();
    bindings_install();
    /* The recorded commands are executed through the console, and a run that never used
       --exec has never registered its table: every replayed command then dispatches to
       nothing, silently, and the replay diverges wherever the recording relied on one.
       The harness does this before its own --exec for the same reason. */
    Console_EnsureRegistered();
    /* Here rather than where the file was opened, because this is the first moment the
       live run is the run that will do the replaying. A recording that reloads pins its
       step and loads its scene between those two points, so comparing at open time
       reported the step and the cube of the session that asked for the replay -- three
       lines of difference, on the reader's own screen, about state the next few frames
       were about to put right. A warning that cries wolf on every mid-session replay is
       worse than no warning, because the one time it is real it reads the same. */
    replay_report_mode();
    s_replaying = 1;
}

int Record_Play(const char *path) {
    char line[640]; /* a binding table is one line and does not fit in 256 */
    char buf[512];
    char newest[ADELINE_MAX_PATH];

    if (s_recording || s_replaying)
        return 0;

    /* No name means the last one recorded, which is what closes the loop: record, stop,
       play, without a path being typed at either end.
     *
     * This run's own recording first, and the folder only when it has none -- a session
     * that starts by replaying something has nothing of its own to prefer. The existence
     * check is for the file being moved or deleted between the two commands, which is a
     * player tidying up rather than a fault, and the scan is a better answer than a
     * "cannot open" naming a file that is gone. */
    if (path == NULL || path[0] == 0) {
        if (s_lastRecorded[0] && ExistsFileOrDir(s_lastRecorded)) {
            path = s_lastRecorded;
        } else if (rec_newest(newest, sizeof newest)) {
            path = newest;
        } else {
            char dir[ADELINE_MAX_PATH];
            GetRecordingsPath(dir, ADELINE_MAX_PATH, NULL);
            /* Names what it looked for, not just where. The scan takes `.rec` only,
               so a session someone saved under another extension is sitting in that
               folder while this says there is nothing in it. */
            Console_Print("rec: no .rec recordings in %s", dir);
            fprintf(stderr, "[rec] no .rec recordings in %s\n", dir);
            return 0;
        }
    }

    path = rec_resolve(path, buf, sizeof buf, 0);
    s_f = fopen(path, "rb");
    if (!s_f) {
        Console_Print("rec: cannot open %s", path);
        return 0;
    }
    snprintf(s_recPath, sizeof s_recPath, "%s", path);

    /* Refuse a file this build cannot read. Growing the tick record without bumping
       this turns an old recording into plausible garbage, which reads as a consistency
       failure at tick 0 rather than as the format mismatch it is. */
    {
        int ver = 0;
        char magic[16] = {0};
        if (fscanf(s_f, "%15s %d", magic, &ver) != 2 || strcmp(magic, REC_MAGIC) != 0) {
            Console_Print("rec: %s is not a recording", path);
            fprintf(stderr, "[rec] %s is not a recording\n", path);
            fclose(s_f);
            s_f = NULL;
            return 0;
        }
        s_recVersion = ver;
        if (ver < REC_VERSION_MIN || ver > REC_VERSION) {
            /* To stderr as well as the console: a run driven from the command line has
               no console to read, and the caller's fallback line is "cannot open",
               which sends the reader looking for a missing file rather than an old
               recording. */
            Console_Print("rec: %s is format %d, this build reads %d..%d", path, ver,
                          REC_VERSION_MIN, REC_VERSION);
            fprintf(stderr, "[rec] %s is format %d, this build reads %d..%d; re-record it\n",
                    path, ver, REC_VERSION_MIN, REC_VERSION);
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

    /* The snapshot the session started from, if this recording carries one. It sits
       between the header and the first record, so it is read here rather than probed
       for in the stream: a reader that reached it through the record dispatcher would
       have to tell a savegame's bytes apart from input, and a savegame contains every
       byte a poll record can. A file without one -- made before the format carried it,
       or by a session that could not stage it -- leaves the position untouched and
       falls through to the sibling below. */
    {
        long at = ftell(s_f);
        S32 reloads = 0;
        RecFmt_ReadInt(s_headerBuf, "setup.reloaded=", &reloads);
        s_inlineSnap[0] = 0;
        if (!replay_read_chunk(REC_SNAP_START, reloads ? s_inlineSnap : NULL,
                               sizeof s_inlineSnap))
            fseek(s_f, at, SEEK_SET);
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
        /* Absent means a recording written before the digest declared itself, which is
           the one field set that predates the line: version 1. Selected before the first
           tick is checked, so the very first comparison is against the set the file was
           written with rather than against this build's. */
        Control_SetDigestVersion(RecFmt_ReadU32(s_headerBuf, "numeric.digest=", &v) ? (S32)v : 1);
        v = 0;
        s_pendingBaseline = 0;
        if (RecFmt_ReadU32(s_headerBuf, "clock.timer_ref_hr=", &v)) {
            s_baseline = v;
            s_pendingBaseline = 1;
        }
        /* The sub-step carry goes back with the clock it is measured against, and for
           the same reason: TimerRefHR says what time it is, this says where inside the
           step the simulation last stopped. Restoring one without the other lands the
           replay's sub-step boundaries a millisecond off the recording's, which is
           invisible for a couple of hundred ticks and then is an actor on a different
           animation. Absent in a recording written before this line existed, which
           leaves the carry as the replay's own -- the behaviour those files were
           recorded under. */
        if (RecFmt_ReadU32(s_headerBuf, "clock.sim_carry=", &v))
            s_baselineCarry = v;
        else
            s_baselineCarry = LastSimRefHR;
    }

    report_extent();

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
            const char *from = NULL;

            /* The file's own copy first, and a sibling only for a recording made before
               the file carried one. Both, rather than the new path alone, because the
               recordings already made are sessions somebody played and there is no
               second run of those to replace them with. */
            if (s_inlineSnap[0]) {
                from = s_inlineSnap;
            } else if (RecFmt_ReadStr(s_headerBuf, "setup.snapshot=", named, sizeof named) &&
                       strcmp(named, "-") != 0 && strcmp(named, "(inline)") != 0) {
                snapshot_beside(path, named, snap, sizeof snap);
                from = snap;
            }

            /* A recording that reloads needs a game to reload into, and the reload is
               issued from MainLoop's tick hook. The menu runs its own loop and never
               reaches that hook, so a playback asked for from the main menu sat armed
               forever while the two lines below reported it had started, and left the
               staged snapshot behind. Measured: `rec info` still said idle six seconds
               later and the scene was still Cube -1, Obj 0.

               Refused here rather than made to work. Playing into the menu means starting
               a game from the recording's snapshot and putting the menu back afterwards,
               which is the demo reel's shape and a larger piece than a guard. */
            if (from != NULL && !Control_HasLiveScene()) {
                Console_Print("rec: %s replays into a game, and none is in progress", path);
                fprintf(stderr, "[rec] %s replays into a game, and none is in progress\n", path);
                if (s_inlineSnap[0]) {
                    remove(s_inlineSnap);
                    s_inlineSnap[0] = 0;
                }
                fclose(s_f);
                s_f = NULL;
                s_recPath[0] = 0;
                return 0;
            }

            if (from != NULL) {
                U32 clk = 0;
                if (RecFmt_ReadU32(s_headerBuf, "setup.reload_clock=", &clk))
                    s_reloadClock = clk;
                /* Save the player's own session before the recording's world is loaded
                   over it. Without this a playback is one-way: the replay ends and the
                   player is left standing wherever the recording finished, in its world
                   rather than theirs. Measured before it existed -- a player who walked
                   away from the recording's end point and then watched it back was put
                   at the recording's end, 1585 units from where they had been.

                   Not for a replay armed from the flag: that run has no player session
                   to protect, it exists to replay and exit. And not where no scene is
                   live, since there is nothing to save. A failure here is reported and
                   the playback still runs, because refusing to play a recording is a
                   worse answer than playing it and saying the way back is missing. */
                s_returnPoint[0] = 0;
                /* s_replayFromCli stays set for the life of the run, unlike its
                   recording counterpart, which clears on use: the replay's exit code is
                   decided from it when the stream ends. So a run given --replay that
                   later plays something from the console takes no return point either.
                   That run is a harness run with no player session in it, and clearing
                   the flag here to change that would take the exit code with it. */
                if (!s_replayFromCli && Control_HasLiveScene()) {
                    char ret[ADELINE_MAX_PATH];
                    if (record_write_snapshot_as(ret, sizeof ret, "return"))
                        snprintf(s_returnPoint, sizeof s_returnPoint, "%s", ret);
                    else
                        Console_Print("rec: could not save your session; "
                                      "playback will not return to it");
                }
                record_reload_request(from, "", REC_RELOAD_PLAY);
                /* After the request, which clears it: the step this replay pins comes
                   from the recording's header, and a session played at some other
                   --fixed-dt reproduces on that one and on nothing else. */
                RecFmt_ReadInt(s_headerBuf, "mode.fixed_dt=", &s_reloadDt);
                Console_Print("rec: reloading %s; replay starts from the post-load state", from);
                return 1;
            }
            Console_Print("rec: %s reloaded a snapshot it does not carry; replaying without it",
                          path);
        }
    }

    replay_begin();
    return 1;
}

/* ---- hooks ---------------------------------------------------------------- */

void Record_PollHook(void) {
    if (s_armed) {
        int kind = s_armed;
        s_armed = 0;
        /* The resolved path on success, the string the player typed on failure: one
           says where the file went, the other is the only thing left to report when
           there is no file -- and where the flag was given no name at all, saying so is
           the only thing left. */
        if (kind == 1) {
            int ok = Record_Start(s_armRecord, 0);
            fprintf(stderr, "[rec] %s %s\n", ok ? "recording to" : "cannot record to",
                    ok ? Record_Path() : (s_armRecord[0] ? s_armRecord : "(no name given)"));
        } else {
            int ok = Record_Play(s_armReplay);
            fprintf(stderr, "[rec] %s %s\n", ok ? "replaying" : "cannot open",
                    ok ? Record_Path() : (s_armReplay[0] ? s_armReplay : "(no name given)"));
        }
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
                    "clock drift max %ld ms first at tick %ld, stream drift first at poll %ld\n",
                    s_polls, s_ticksChecked, s_hashMismatchTick, s_refDriftMax,
                    s_refDriftFirstTick, s_syncDriftPoll);
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
        if (!Control_LoadSettled())
            return; /* ChangeCube has not consumed it yet */
        record_reload_landed();
        s_reloadState = 0;
        if (s_reloadAbort) {
            s_reloadAbort = 0;
            /* A playback stopped between `rec play` and its reload landing. The load
               cannot be taken back -- the engine changes cube either way -- so the player
               is standing in the recording's world with the session called off, and
               Record_Stop could not ask for the way back because this reload was still in
               flight when it ran. Measured: stopping in that window left the hero at the
               recording's start rather than where the playback found him. Ask here
               instead, where the state machine is free again. */
            record_restore_player();
        } else
            s_beginAtPoll = (s_reloadForPlay == REC_RELOAD_RESTORE)
                                ? 0
                                : (s_reloadForPlay == REC_RELOAD_PLAY ? 2 : 1);
        return;
    }

    if (!s_recording && !s_replaying)
        return;
    h = Control_StateDigest();

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
        if ((s_ticks % REC_KEY_EVERY) == 0 && record_keyframe_fit()) {
            S32 kf[REC_KF_MAX];
            S32 nkf = Control_KeyframeCount();
            S32 k;
            Control_KeyframeFill(kf);
            put8(REC_KEY);
            put32((U32)s_ticks);
            put16((U16)nkf);
            for (k = 0; k < nkf; k++)
                put32((U32)kf[k]);
        }
        if (s_teleWrite) {
            /* Filled by the Control_StateDigest call above, so this is the state the
               digest just committed to rather than a second read of it. */
            S32 k, n = Control_StateValueCount();
            put8(REC_TELE);
            put32((U32)s_ticks);
            put16((U16)n);
            for (k = 0; k < n; k++)
                put32((U32)Control_StateValue(k));
            if (!s_teleSaid) {
                s_teleSaid = 1;
                fprintf(stderr, "[rec] verbose telemetry: %d values a tick\n",
                        (int)Control_StateValueCount());
            }
        }
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
        /* A command issued from the console is written where it ran, which is the input
           path, so it lands after the last poll of the tick and before the tick's own
           record. Step over it here rather than giving up on the tick: giving up left the
           tick unchecked and the record was then read as a poll, which lost the stream on
           the first command a player typed. It runs at the bottom of this tick with the
           rest. */
        while (flags == 0x40) {
            U32 ct;
            U16 n, j;
            char line[sizeof s_cmds[0].line];
            if (!get32(&ct) || !get16(&n))
                return;
            for (j = 0; j < n; j++) {
                U8 c;
                if (!get8(&c))
                    return;
                if ((size_t)(j + 1) < sizeof line)
                    line[j] = (char)c;
            }
            line[((size_t)(n + 1) < sizeof line) ? n : sizeof line - 1] = 0;
            if (s_nCmds < REC_CMD_MAX) {
                s_cmds[s_nCmds].tick = ct;
                snprintf(s_cmds[s_nCmds].line, sizeof s_cmds[s_nCmds].line, "%s", line);
                s_nCmds++;
            }
            pos = ftell(s_f);
            if (!get8(&flags))
                return;
            if (!replay_take_sync(&flags))
                return;
        }
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

        /* A keyframe, if this tick carries one: compare it field by field and name what
           moved. The hash has already said whether this tick matches; this says why,
           which is the part a session nobody can re-record otherwise cannot answer. */
        {
            long pk = ftell(s_f);
            U8 fk;
            if (get8(&fk)) {
                if (fk == REC_KEY && record_keyframe_fit()) {
                    U32 kt;
                    S32 live[REC_KF_MAX];
                    S32 nkf = 0, shared;
                    S32 k, shown = 0;
                    get32(&kt);
                    if (!replay_keyframe_count(&nkf))
                        nkf = 0;
                    /* Read every field the record says it holds and compare the ones both
                       ends have. Fields are appended, so the names line up from the front;
                       what must not happen is reading fewer than were written, which leaves
                       the tail of this record to be taken as the next one. */
                    shared = nkf < Control_KeyframeCount() ? nkf : Control_KeyframeCount();
                    Control_KeyframeFill(live);
                    for (k = 0; k < nkf; k++) {
                        U32 rv = 0;
                        if (!get32(&rv))
                            break;
                        if (k >= shared)
                            continue;
                        if ((S32)rv == live[k])
                            continue;
                        /* Only once the run has stopped matching. Before that a
                           difference here would be a keyframe bug, and after the first
                           report every later keyframe differs, so cap the noise. */
                        if (s_hashMismatchTick < 0 || s_kfReported >= 3)
                            continue;
                        if (!shown) {
                            fprintf(stderr, "[rec] tick %u differs:", (unsigned)kt);
                            shown = 1;
                        }
                        fprintf(stderr, " %s %d/%d", Control_KeyframeName(k), (int)rv, (int)live[k]);
                    }
                    if (shown) {
                        fprintf(stderr, "  (recorded/replayed)\n");
                        s_kfReported++;
                    }
                } else {
                    fseek(s_f, pk, SEEK_SET);
                }
            }
        }

        /* Verbose telemetry, if the recording carries it: the same values the digest
           mixed, so the tick that failed can name the fields that moved instead of
           leaving ~1300 of them to choose between. Reported only on a tick the digest
           already rejected -- before that a difference here would be a telemetry bug,
           and after the first few every later tick differs. */
        {
            long pt = ftell(s_f);
            U8 ft;
            if (get8(&ft)) {
                if (ft == REC_TELE) {
                    U32 tt;
                    U16 tn, k;
                    S32 shown = 0;
                    S32 nlive = Control_StateValueCount();
                    /* The first rejected tick, and the two after it: one tick says
                       what moved, three say whether it is spreading. */
                    int say = (s_hashMismatchTick >= 0 && s_teleReported < 3);
                    if (get32(&tt) && get16(&tn)) {
                        if (say && (S32)tn != nlive) {
                            /* A different number of values is a different world: the
                               actor list is not the same length, and comparing them
                               position by position would report nonsense. */
                            fprintf(stderr,
                                    "[rec] tick %u holds %d values, this run has %d: "
                                    "the actor list differs\n",
                                    (unsigned)tt, (int)tn, (int)nlive);
                            s_teleReported++;
                            say = 0;
                        }
                        for (k = 0; k < tn; k++) {
                            U32 rv = 0;
                            if (!get32(&rv))
                                break;
                            if (!say || (S32)k >= nlive)
                                continue;
                            if ((S32)rv == Control_StateValue((S32)k))
                                continue;
                            if (shown >= 8)
                                continue;
                            {
                                char nm[64];
                                Control_StateValueName((S32)k, nm, sizeof nm);
                                if (!shown)
                                    fprintf(stderr, "[rec] tick %u state differs:", (unsigned)tt);
                                fprintf(stderr, " %s %d/%d", nm, (int)(S32)rv,
                                        (int)Control_StateValue((S32)k));
                            }
                            shown++;
                        }
                        if (shown) {
                            fprintf(stderr, "  (recorded/replayed)\n");
                            s_teleReported++;
                        }
                    }
                } else {
                    fseek(s_f, pt, SEEK_SET);
                }
            }
        }

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
            for (j = 0; j < n && (size_t)(j + 1) < sizeof line; j++) {
                U8 c;
                if (!get8(&c))
                    break;
                line[j] = (char)c;
            }
            line[j] = 0;
            s_cmdsReplayed++;
            Console_Execute(line);
        }

        /* Anything the poll reader queued: a command typed at the console is written
           mid-poll, and running it there would re-enter the stream reader. */
        while (s_cmdRead < s_nCmds) {
            const char *queued = s_cmds[s_cmdRead].line;
            s_cmdRead++;
            s_cmdsReplayed++;
            Console_Execute(queued);
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

const char *Record_Path(void) { return s_recPath; }
int Record_IsReplaying(void) { return s_replaying; }
int Record_IsRecording(void) { return s_recording; }

/* ---- compression ---------------------------------------------------------- */

/* The engine already ships LZSS (SOURCES/LZSS.CPP), used for savegames and the HQR
   containers. Measures what it would give on a recording's payload rather than
   guessing, and leaves the file alone. */
unsigned long Record_Squeeze(const char *path) {
    char pbuf[512];
    FILE *f;
    path = rec_resolve(path, pbuf, sizeof pbuf, 0);
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

    /* The savegames at each end are not the record stream, and this measures how well
       the record stream packs. They are already compressed by the save layer, so
       leaving them in answers a question nobody asked with a number that looks like an
       answer to this one. */
    {
        U8 frame[REC_CHUNK_TAIL];
        U8 op = 0;
        U32 len = 0;
        if (fseek(f, hdr, SEEK_SET) == 0 &&
            fread(frame, 1, REC_CHUNK_HEAD, f) == REC_CHUNK_HEAD &&
            RecFmt_GetChunkHead(frame, REC_CHUNK_HEAD, &op, &len) && op == REC_SNAP_START &&
            len <= REC_SNAP_MAX)
            hdr += REC_CHUNK_HEAD + (long)len + REC_CHUNK_TAIL;
        if (total > (long)sizeof frame && fseek(f, total - (long)sizeof frame, SEEK_SET) == 0 &&
            fread(frame, 1, sizeof frame, f) == sizeof frame &&
            RecFmt_ReadChunkTailLen(frame, sizeof frame, &len) && len <= REC_SNAP_MAX) {
            long at = total - (long)sizeof frame - (long)len - REC_CHUNK_HEAD;
            if (at > hdr)
                total = at;
        }
    }

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
    Console_Print("rec: %s stream %ld, LZSS %lu (%.1f%% of it); the rest of the file is "
                  "the header and the savegames",
                  path, payload, packed, 100.0 * (double)packed / (double)payload);
    free(in);
    free(out);
    return packed;
}

/* ---- reporting ------------------------------------------------------------ */

void Record_Report(const char *path) {
    char live[REC_HEADER_MAX];
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
        char pbuf[512];
        FILE *f;
        char line[640];
        /* Sized against the buffer the header is built in rather than a round number:
           the header grows whenever a setting joins it, and a `hdr` that silently
           dropped the tail would leave those lines out of the comparison and report a
           match for settings it never looked at. */
        char hdr[sizeof live];
        int diffs = 0;
        size_t used = 0;
        path = rec_resolve(path, pbuf, sizeof pbuf, 0);
        f = fopen(path, "rb");
        if (!f) {
            Console_Print("rec: cannot open %s", path);
            return;
        }
        hdr[0] = 0;
        if (!fgets(line, sizeof line, f)) { /* magic */
            Console_Print("rec: %s is empty", path);
            fclose(f);
            return;
        }
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
            {
                size_t want = strlen(line) + 1;
                if (used + want + 1 > sizeof hdr) {
                    Console_Print("rec: header longer than this build can compare; "
                                  "the lines after `%s` are not checked",
                                  line);
                    break;
                }
                memcpy(hdr + used, line, want - 1);
                used += want - 1;
                hdr[used++] = '\n';
                hdr[used] = 0;
            }
        }
        fclose(f);

        /* Diff line by line against the live run. */
        {
            char *p = hdr, *nl;
            while ((nl = strchr(p, '\n')) != NULL) {
                *nl = 0;
                if (p[0] && !line_present(live, p)) {
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
