#ifndef CONTROL_H
#define CONTROL_H

/*
 * CLI control harness: drive the engine from outside for automation / regression.
 *
 *   lba2 --load <slot> --exec "<cmd>;<cmd>" --tick <N> --fixed-dt <ms> --demo \
 *        --dump-state <path> --screenshot <path> --exit
 *
 * The harness reuses existing engine seams rather than adding new ones: the console
 * command bus (Console_Execute) for --exec, the InitGame(-1) -> ChangeCube() load
 * sequence for --load, SavePNG for --screenshot, and plain global reads for
 * --dump-state. The only engine intrusions are the three call sites in PERSO.CPP
 * (parse, boot branch, and the one tick hook in MainLoop). See docs/CONTROL.md and
 * docs/plan/AUTOMATION_PLAN.md.
 */

#include <SYSTEM/ADELINE_TYPES.H>

#include <stddef.h> /* size_t, for the state-value name buffer */

#ifdef __cplusplus
extern "C" {
#endif

/* Parse + strip harness flags from argv. Call early in main(), mirroring
   parse_save_load_test_flag: recognised flags and their values are removed in place
   so downstream positional parsing is unaffected. */
void Control_ParseArgs(int *argc, char *argv[]);

/* Non-zero if any harness flag was supplied this run. */
int Control_IsActive(void);

/* Non-zero if --headless was supplied: no window, no audio device. */
int Control_IsHeadless(void);

/* Which set of fields Control_StateDigest hashes. A replay sets it from the recording's
   `numeric.digest` line so a file written against the older set still compares against
   the set it was written with; a recording always uses the current one. Fields added
   since are appended, never interleaved, so selecting the older version reproduces its
   sequence exactly rather than approximating it, and a field that stops being compared
   stays compared at the versions that compared it. */
#define CONTROL_DIGEST_VERSION 2
/* Select the set to hash. A version above CONTROL_DIGEST_VERSION is one this build does
   not have the fields for; returns 0 for it and leaves the selection alone, so a caller
   can refuse the file by name instead of hashing today's set under tomorrow's number and
   reporting the difference as a simulation that diverged on its first tick. */
S32 Control_SetDigestVersion(S32 v);

/* What a digest field is, and therefore whether a mismatch in it is a divergence.
 *
 * The digest answers "did the simulation reach the same state", so a field belongs in
 * the comparison only if a replay can be made to start from the same value. For every
 * field, one of these has to be true:
 *
 *   DIGEST_SAVED    the replay's load restores it -- either the savegame carries the
 *                   bytes, or the load recomputes it from what it does carry, which is
 *                   how Island, CubeMode, NbObjets and NbZones arrive
 *   DIGEST_CARRIED  the savegame does not restore it, so the recording carries it
 *                   beside the savegame and the replay installs it for its duration
 *   DIGEST_LOOSE    nothing establishes it in the replay, which starts it at its
 *                   GLOBAL.CPP initialiser: collected and named, never compared
 *   DIGEST_PROBE    not state at all -- a measurement of how the run got here, such as
 *                   the RNG draw count. Reported beside the fields, never compared
 *
 * A field in none of these is the defect the classes exist to stop: it is compared
 * against a value no replay can establish, so it reports a divergence with nothing
 * having diverged, and an oracle that cries wolf gets ignored. The class is an argument
 * to the mixing macro rather than a comment beside it because that is what stops the
 * next field being added without the question being answered.
 *
 * DIGEST_LOOSE is a diagnosis and not a destination. A field lands there because it
 * reaches the simulation and nothing carries it, which is a gap in the recording rather
 * than a property of the field; the repair is to carry it and move it to
 * DIGEST_CARRIED, on the same borrow-and-return the binding tables already use. */
#define DIGEST_SAVED 1
#define DIGEST_CARRIED 2
#define DIGEST_LOOSE 3
#define DIGEST_PROBE 4

/* Write the same JSON snapshot --dump-state writes, to \a path, right now. The flag
   captures once as the run ends, which is all a bounded run needs; a session driven
   from outside wants one per probe, so the console `dumpstate` verb routes here. */
void Control_DumpStateTo(const char *path);

/* --listen: open the command socket if this run asked for one, then service it.
   Call once per presented frame (PERSO.CPP's pre-present chain) rather than per
   tick, because the tick hook does not run inside a modal's own loop, which is where a
   connected driver would otherwise go silent. No-op without --listen. */
void Control_ServiceListen(void);

/* Number of --exec-at entries whose tick the run never reached, reported on stderr as a
   side effect. Non-zero means the run did not do what was asked of it. */
int Control_UnfiredExecAt(void);

/* Non-zero if --verbose was supplied: log scene/hero progress and modal entries to stderr,
   so a headless run is self-explaining and a hang shows what blocked it. This is all it
   does -- the recording telemetry has its own flag below, because the two have nothing to
   do with each other and one of them costs megabytes. */
int Control_IsVerbose(void);

/* Non-zero if --record-telemetry was supplied: a recording made by this run stores every
   value the state digest mixes, every tick, so a divergence names the field rather than
   only the tick. About 2 KB a tick (docs/RECORDING.md). Named for the recording rather
   than called --telemetry on its own, which in a game reads as phone-home analytics and
   is not what this is: nothing leaves the machine. */
int Control_RecordTelemetry(void);

/* Log verbosity via --log-level <debug|info|warn|error>. Control_HasLogLevel is
   non-zero if the flag was supplied; Control_GetLogLevel returns the LogSeverity
   value (as int) to hand to Log_SetLevel. Applied in PERSO.CPP right after
   Log_Init; when absent the engine falls back to LBA2_LOG_LEVEL, then INFO. */
int Control_HasLogLevel(void);
int Control_GetLogLevel(void);

/* Resolution override via --resolution WxH. Returns 1 if the flag was supplied (use
   Control_GetResolutionX/Y), 0 otherwise (use the compile-time RESOLUTION_X/RESOLUTION_Y
   defaults). Parsed in Control_ParseArgs; safe to call after that. */
int Control_HasResolution(void);
long Control_GetResolutionX(void);
long Control_GetResolutionY(void);

/* Language override via --language <name>. Returns 1 if the flag was supplied (use
   Control_GetLanguage as a 0..5 index into MESSAGE.CPP's TabLanguage[]), 0 otherwise
   (the engine reads the cfg's Language key as normal). Accepts full names
   (English / Français / Deutsch / Español / Italiano / Portugues, case-insensitive on
   ASCII letters) and 2-character codes (EN / FR / DE / SP / IT / PO). Parsed in
   Control_ParseArgs; applied in PERSO.CPP after InitLanguage() returns. Doesn't
   touch the cfg file. */
int Control_HasLanguage(void);
int Control_GetLanguage(void);

/* Sim-throttle override via --fixed-timestep <ms>. Returns 1 if the flag was supplied
   (use Control_GetFixedTimestep for the forced value, 0..100, 0 = off), 0 otherwise.
   Applied on the first tick in Control_TickHook, after ReadConfigFile has taken the
   cfg's value. WriteConfigFile reads these back so the forced value is not persisted:
   the flag is documented as this run only, unlike the `fixedtimestep` console verb. */
int Control_HasFixedTimestep(void);
long Control_GetFixedTimestep(void);

/* Vsync override via --vsync <on|off>. Returns 1 if the flag was supplied (use
   Control_GetVSync for the forced value, 0 or 1), 0 otherwise (the engine reads the
   cfg's VSync key as normal). Applied in PERSO.CPP where ReadConfigFile takes that key;
   WriteConfigFile reads these back so the forced value is not persisted, unlike the
   `vsync` console verb and the Display menu's toggle.

   This is the setting the Display submenu prints, which is why a UI capture needs to be
   able to name it. Frame pacing is a separate matter: Control_Begin drops the
   presentation cap for every --exit run whatever this says. */
int Control_HasVSync(void);
int Control_GetVSync(void);

/* --no-audio: skip InitAIL() and InitSampleDriver() at boot. Returns 1 if the flag
   was supplied, 0 otherwise. With audio init skipped, the SDL audio subsystem
   never opens a stream and the dummy driver's nanosleep pacing (~74% of sys time
   in projection_demo on a WSL2 setup) doesn't run. Every audio entry point in
   LIB386/AIL/SDL/{SAMPLE,STREAM,VIDEO_AUDIO_SDL}.CPP already gates on
   Sample_Driver_Enabled or a non-NULL stream, so playback calls become silent
   no-ops. Harness-only intent; players who actually want audio shouldn't pass
   this flag. */
int Control_NoAudio(void);

/* Whether a sample driver is actually up, which is not the same question as the flag
   above: --headless implies --no-audio, and InitSampleDriver can fail on a host with no
   audio device and let the run continue silently (SOURCES/INITADEL.C). All three land in
   the same place, and the simulation reads that place, so a recording carries this
   answer rather than the flag that usually produces it. */
int Control_AudioActive(void);

/* Set up the boot path and arm the tick hook. Call once, instead of MainGameMenu(),
   when active: applies --load (InitGame(-1) + ChangeCube while FlagLoadGame is set)
   or a fresh InitGame(1), then arms Control_TickHook. Returns 1 on success (caller
   should run MainLoop), 0 on failure such as an unresolvable --load (caller should
   skip MainLoop and exit). */
int Control_Begin(void);

/* Single seam invoked at the top of the MainLoop body. No-op (one flag test) until
   armed. Runs the --exec batch on the first tick (scene loaded by then), and after N
   effective ticks finalizes (--dump-state, --screenshot) and, if --exit, exits via
   TheEnd(). */
void Control_TickHook(void);
void Control_ArmRecording(void); /* prototype: --record / --replay */

/* Capture final --dump-state / --screenshot when MainLoop returns naturally (e.g. the
   demo script reaches LM_THE_END before the --tick budget). Call once between MainLoop()
   and TheEnd() on the harness boot path. No-op if the tick-budget finalize already ran,
   the harness is not active, or no capture flag was supplied. */
void Control_End(void);

/* --- Harness input timeline (docs/plan/INPUT_SIM_PLAN.md) -----------------------
 *
 * Drives the hero from the console `input` command / --exec with holds and sequences metered in
 * SIM TICKS, not rendered frames. That is the point: a rendered frame the fixed-timestep throttle
 * skips consumes no tick, so a hold means the same amount of gameplay whatever --fixed-timestep is
 * (with the old render-frame count, `input up 30` walked full distance at --fixed-timestep 16 and
 * zero at 100). The current mask is OR'd into Input in MainLoop, so injected input rides the same
 * path as real input. All durations are sim ticks.
 */

/* Clear the timeline (console `input off`). */
void Control_InputClear(void);

/* Append `mask` held from sim tick `startTick` for `durTicks` ticks. Returns 1 on success, 0 if
   the event table is full. A zero mask or zero duration is a no-op gap. Builds `input seq`. */
int Control_InputSchedule(U32 startTick, U32 durTicks, U32 mask);

/* Like Control_InputSchedule but measured in RENDERED frames, not sim ticks. This is the tool for
   reproducing the throttle input-EDGE drop: a press placed on a throttle-skipped frame (a frame
   the sim does not step) is never seen by the object loop, exactly as a real high-refresh display
   drops it. Builds `input fseq`. */
int Control_InputScheduleFrame(U32 startFrame, U32 durFrames, U32 mask);

/* --- Harness key holds (console `key`) -------------------------------------
 *
 * Hold a raw scancode down at the keyboard layer, which is the only injection point a
 * MODAL sees: the timeline above reaches Input from MainLoop, and a menu or dialogue
 * spinning in its own MyGetInput loop never gets back there. Every such loop does pass
 * through ManageKeyboard, where ApplyHarnessKeys applies these.
 *
 * Durations are input polls, one per ManageKeyboard, not sim ticks: a modal advances no
 * ticks and a tick-based delay inside one would never elapse. A poll is not the same as
 * a MyGetInput call, since InitWaitNoInput(x) is GetInput(x) and that runs ManageKeyboard
 * too. In MainLoop, where nothing else polls, one poll is one rendered frame.
 */

/* One past the highest scancode the packed joystick tail can address. CheckKey indexes
   the tail as 256 - 32 + (sc >> 3), so the array bound, not the bit count, fixes this. */
#define CONTROL_KEY_MAX_SCANCODE ((TABKEYS_NUM_KEYS - (256 - 32)) * 8)

/* Hold `scancode` for `durPolls` polls, starting `delayPolls` polls from now. Returns 1,
   or 0 if the scancode is out of range or the hold table is full. Scancode 0 is rejected:
   it is SDL_SCANCODE_UNKNOWN and also the filler in every unset binding slot. Gamepad
   scancodes (K_GAMEPAD_*) are accepted and land in the joystick tail. */
int Control_KeyHold(U32 scancode, U32 delayPolls, U32 durPolls);

/* Drop every scheduled hold (console `key off`). */
void Control_KeyClear(void);

/* Polls elapsed so far, so a caller can report timing against the same clock. */
U32 Control_KeyPollCount(void);

/* Move the mouse by (dx, dy) every poll for the next `durPolls` polls, with `buttons`
   held down over the same span (the Click mask: 1 left, 2 right, 4 middle). Returns 1,
   or 0 for a zero-length hold. One at a time: a second call replaces the first, because
   the accumulator has no notion of two mice. No delay argument, unlike the key holds:
   --exec-at already places a command on a tick, and nothing wanted a second way.

   The analog camera is the reason this exists. It reads MouseXDep/MouseYDep and Click,
   none of which is a scancode, so Control_KeyHold cannot reach it. */
int Control_MouseMove(S32 dx, S32 dy, U32 buttons, U32 durPolls);

/* Stop the motion and lift the buttons it held (console `mouse off`). */
void Control_MouseClear(void);

/* Hold the right stick at (x, y) in SDL axis units for the next `durPolls` polls,
   reporting a pad present for as long as it runs. Returns 1, or 0 for a zero-length
   hold. One at a time, as with the mouse.

   The axes are cleared and rederived on every poll, so this re-applies on each rather
   than setting once, and a real pad plugged in at the same time will overwrite it. */
int Control_StickHold(S16 x, S16 y, U32 durPolls);

/* Centre it and stop reporting a pad (console `stick off`). */
void Control_StickClear(void);

/* Replace the timeline with a single hold of `mask` for `durTicks` ticks
   (console `input <flags> [ticks]`). */
void Control_InputHold(U32 mask, U32 durTicks);

/* OR of every event held at the current sim tick. MainLoop ORs this into Input each frame while
   Control_InputPending(). */
U32 Control_InputCurrentMask(void);

/* Non-zero while a scheduled event has not yet elapsed. */
int Control_InputPending(void);

/* Advance the timeline by `simTicks` (the sim steps a frame ran). Call once per sim-running frame
   in MainLoop; skipped frames must not call it. */
void Control_InputAdvance(int simTicks);

/* Advance the rendered-frame clock. Call once per rendered frame (skipped frames included) so a
   frame-based (`fseq`) event lands on the exact frame requested. */
void Control_InputFrameTick(void);

/* Toggle a per-sim-tick trace of the injected mask + hero gen_anim/pos to the debug log
   (console `input trace <0|1>`), so a headless run can watch the hero respond. */
void Control_InputTrace(int on);

/* ---- the game state the harness reads --------------------------------------
 *
 * This module already reads the engine's globals to write --dump-state, and it is the
 * one place that includes C_EXTERN.H for that. Everything else asks through here, so a
 * second module does not double a dependency the build graph holds down
 * (scripts/ci/check-build-graph.py rule 2), and so the walk over the state exists once.
 *
 * Three views of one walk. The digest answers "did this tick match"; the values behind
 * it, named, answer "which field moved"; the keyframe is the short list a person reads
 * first. Calling the digest refills the values, so the two always describe the same
 * moment. */

/* FNV-1a over scene, hero, camera, every other actor, the open modal, and all script
   variables. Refills the value table below as it goes. */
unsigned long long Control_StateDigest(void);

/* The values the digest just mixed, in the order it mixed them. Valid until the next
   Control_StateDigest call. */
S32 Control_StateValueCount(void);
S32 Control_StateValue(S32 k);
/* Non-zero if value \a k is part of the verdict at the version being compared, rather
   than collected beside it. A reporter that does not ask this says "state differs" over
   a field the hash never looked at, which is the false alarm the classes exist to end,
   moved from the digest into the text. */
S32 Control_StateValueCompared(S32 k);
/* "hero->Obj.X", "var.game[77]", "obj[12].Anim". The name comes from the expression the
   digest mixes, so a field cannot be hashed under one name and reported under another. */
void Control_StateValueName(S32 k, char *out, size_t n);

/* The short named keyframe: where the hero is, what he is doing, where the camera is,
   and what modal is up. */
S32 Control_KeyframeCount(void);
void Control_KeyframeFill(S32 *v);
const char *Control_KeyframeName(S32 k);
/* Non-zero if keyframe field \a k is part of the verdict, on the same question
   Control_StateValueCompared answers for the digest's own values. */
S32 Control_KeyframeCompared(S32 k);

/* Is there a scene loaded to read? Boot and the menus have neither cube nor actors. */
S32 Control_HasLiveScene(void);

/* The part of a run's description that comes from the engine: where the game is, and
   the settings a replay has been shown to turn on, as `key=value` lines. */
S32 Control_DescribeState(char *out, size_t n);

/* ---- the state a savegame does not carry ------------------------------------ */

/* The DIGEST_CARRIED globals, as `state.` header lines. Returns 0 if the buffer was too
   small, on the same argument as the rest of the header: a truncated set installs some
   of the fields and leaves the others at this run's values, which is a state neither
   the recording nor the replay was ever in. */
S32 Control_WriteUncarried(char *out, size_t n);

/* Install those values out of a recording's header for the replay's duration, keeping
   the run's own to hand back. All or nothing: returns 0 for a header that does not
   carry the whole set, which is every recording written before it existed, and installs
   nothing in that case. Idempotent, so a re-arm does not overwrite the held copy with
   the values it installed last time. */
S32 Control_InstallUncarried(const char *hdr);

/* Hand the run's own back. A no-op if nothing is held. */
void Control_RestoreUncarried(void);

/* ---- savegames the harness drives ------------------------------------------ */

/* Write a savegame to `path` without disturbing the run: SaveGame writes "the current
   game", so this borrows the save context and puts it back. Returns 0 for a path it
   will not write. */
S32 Control_WriteSnapshot(const char *path);

/* The same borrow, for a caller that drives a load itself and has to put the run's own
   save identity back afterwards. Borrow is idempotent; return is a no-op if nothing is
   held. */
void Control_BorrowSaveContext(void);
void Control_ReturnSaveContext(void);

/* Clear the engine's pending-load flag after a load driven from outside MainLoop's
   restart path, which is the only thing that would otherwise clear it. */
void Control_ClearLoadFlag(void);

/* Ask the engine to load `path`. It lands on MainLoop's `NewCube != -1` block at the
   top of the next tick, so poll Control_LoadSettled to know when it has. */
void Control_RequestLoad(const char *path);
S32 Control_LoadSettled(void);

/* The save-format version the engine is holding, for a caller that has to restore it
   around a load it drove. */
S32 Control_SaveVersion(void);
void Control_SetSaveVersion(S32 v);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */
